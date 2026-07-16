# PixelDisplay Pro — Software Design Document

**Version:** 0.1 (Design Draft — pre-implementation)
**Target hosts:** Adobe After Effects (primary), Premiere Pro & OpenFX (future)
**Language:** C++20
**Status:** Awaiting design approval before any implementation begins.

---

## 0. Executive Summary

PixelDisplay Pro is a commercial-grade After Effects plugin that **physically re-synthesizes** an image as if it were being reproduced by real display hardware. Rather than overlaying a halftone/dot pattern, the plugin rebuilds each output pixel from a *virtual display model*: it divides the frame into a lattice of emissive display cells, decomposes each cell into subpixels with a physically-motivated geometry (RGB stripe, PenTile, Diamond, shadow-mask, aperture-grille, etc.), drives those subpixels from the resampled source signal, and then layers in the optical and electronic imperfections that make a captured photo of a screen look like a photo of a screen (bloom, mura, PWM flicker, rolling shutter, burn-in, moiré against the camera grid, glass reflection/refraction).

The core design principle is **separation of engine from host**. The rendering engine is a self-contained C++20 library with zero dependency on the After Effects SDK. The AE plugin is a thin *host adapter* that translates AE's parameter/render callbacks into engine calls and hands buffers back and forth. Porting to Premiere or OpenFX means writing a new adapter — never touching the engine.

This document specifies the architecture, pipeline, folder layout, class model, GPU and CPU paths, full parameter set, data flow, memory ownership, thread-safety and Multi-Frame-Rendering (MFR) contract, performance strategy, and the milestone plan. **No code is written until this design is approved.**

---

## 1. Design Goals & Non-Goals

### 1.1 Goals
- **Physical plausibility.** Output should survive being called "a photo of a real screen" by a colorist.
- **Host independence.** Engine compiles and unit-tests with no Adobe headers present.
- **GPU-first, CPU-correct.** Metal → D3D12 → OpenGL priority; a CPU fallback that produces *numerically equivalent* results (within tolerance) always exists.
- **MFR-safe.** Fully re-entrant; any number of frames render concurrently with no shared mutable state.
- **Deterministic.** Same parameters + same frame + same time ⇒ same pixels, on GPU and CPU, across runs. All "randomness" is seeded and coordinate-derived.
- **Cache-driven.** Procedural geometry, LUTs, and masks are generated once per parameter-set and reused across frames and tiles.
- **Production robustness.** No leaks, graceful GPU-loss recovery, bounded memory, predictable performance.

### 1.2 Non-Goals (v1)
- Not a general compositing/OpenGL-panel plugin; it is an effect (filter) plugin.
- Not a real-time game post-process (though the engine is structured to allow it later).
- No network features, no telemetry, no external asset downloads at render time.

---

## 2. Overall Architecture

Two hard layers, one direction of dependency.

```
┌──────────────────────────────────────────────────────────────────┐
│  LAYER 1 — HOST ADAPTER  (knows AE, knows nothing about GPU/pixels)│
│                                                                    │
│   AE Entry Point  ──►  Parameter Model  ──►  ParamSnapshot (POD)   │
│   AE UI / Presets      Preset (De)Serializer                        │
│   AE Render CB    ◄──  ImageBuffer exchange (borrowed pointers)     │
└───────────────────────────────┬────────────────────────────────────┘
                                 │  depends on ▼   (engine never calls up)
┌───────────────────────────────┴────────────────────────────────────┐
│  LAYER 2 — PIXELDISPLAY ENGINE  (no Adobe headers, standalone lib)   │
│                                                                      │
│   RenderContext ─► FrameGraph ─► [Stages] ─► Backend (GPU|CPU)       │
│   ResourceCache (LUTs, layout geometry, masks)                       │
│   DisplayModel registry · Artifact modules · Lens · Color · Sampling │
└──────────────────────────────────────────────────────────────────────┘
```

**Dependency rule:** `Host/` may include `Engine/` public headers. `Engine/` may include **nothing** from `Host/` and nothing from Adobe. This is enforced by CMake target boundaries (the engine target does not list the AE SDK include dirs) and by a CI header-hygiene check.

**Interface surface.** The host talks to the engine through exactly three POD/handle types:
- `ParamSnapshot` — a flat, versioned, copyable struct holding *all* effect parameters for one render (no pointers into host memory except explicitly-owned mask blobs).
- `ImageView` — a non-owning descriptor of a pixel buffer (ptr/rowbytes/width/height/format/colorspace). Ownership stays with whoever created it.
- `Engine` — an opaque facade with `render(const RenderRequest&)`.

Everything else (backends, stages, caches, display math) is engine-internal.

---

## 3. Folder Structure

```
PixelDisplay/
├── CMakeLists.txt                 # top-level: adds Engine, Host, Tests
├── Engine/                        # Layer 2 — standalone static lib `pdengine`
│   ├── CMakeLists.txt
│   ├── Core/                      # RenderContext, FrameGraph, Stage, Engine facade
│   ├── Renderer/                  # Backend abstraction, FrameGraph executor
│   │   ├── GPU/
│   │   │   ├── Metal/             # MTL backend (.mm, compiled on macOS)
│   │   │   ├── D3D12/             # DX12 backend (Windows)
│   │   │   ├── OpenGL/            # GL 4.3+ / GLES3.1 fallback backend
│   │   │   └── Shaders/           # .metal / .hlsl / .comp sources + generated headers
│   │   └── CPU/                   # scalar + SIMD reference backend
│   ├── DisplayLayouts/            # procedural layout generators (LCD/OLED/CRT/...)
│   ├── Artifacts/                 # dead/stuck/hot pixels, mura, burn-in, banding...
│   ├── Lens/                      # reflection, refraction, defocus, curvature, moiré
│   ├── Animation/                 # scanlines, rolling refresh, PWM, response, shutter
│   ├── Math/                      # vec/mat, transforms, easing, hash, fixed helpers
│   ├── Sampling/                  # source resampling kernels (box/bilinear/Lanczos)
│   ├── Noise/                     # value/perlin/blue-noise, seeded generators
│   ├── Color/                     # colorspaces, transfer fns, LUT builders
│   └── Utilities/                 # Result<T>, Span, aligned alloc, logging, profiling
├── Host/                          # Layer 1
│   └── AfterEffects/
│       ├── CMakeLists.txt         # builds the .aex/.plugin, links pdengine + AE SDK
│       ├── EntryPoint/            # PF_Cmd dispatch, GlobalSetup, SmartFX callbacks
│       ├── Parameters/            # param registration, ParamSnapshot builder
│       ├── UI/                    # custom UI / Effect panel, group collapsing
│       ├── Presets/              # preset (de)serialization, FFX generation
│       └── Resources/            # PiPL, strings, icons
├── Tests/                         # engine unit + golden-image tests (no AE)
├── Documentation/                 # this doc + per-subsystem docs
└── cmake/                         # toolchain files, SDK-find modules, packaging
```

Rationale: the tree mirrors the pipeline stages so a new contributor can map "stage 9 = Lens effects" directly to `Engine/Lens/`. Shaders live beside the backend that consumes them but are authored once in a shared `.pdsl`-style comment convention (see §7.4).

---

## 4. Rendering Pipeline

The engine executes a **FrameGraph**: an ordered list of stages, each declaring its input/output resources. Stages are pure functions of `(ParamSnapshot, RenderContext, inputs) → outputs`. The graph is compiled once per `ParamSnapshot` (cheap) and dispatched per frame.

Per-frame stage order (matches the spec's 13 steps):

| # | Stage | Module | GPU kind | Notes |
|---|-------|--------|----------|-------|
| 1 | **Ingest** source | Core | copy/upload | wrap host buffer → engine texture (or borrow) |
| 2 | **Build display grid** | DisplayLayouts | (cached) | virtual cell lattice; regenerated only on param change |
| 3 | **Resample** source | Sampling | compute | sample source at cell centroids / subpixel taps |
| 4 | **To linear** | Color | compute | input transfer → linear working space |
| 5 | **Generate pixels** | Renderer | compute | emit per-cell emission from sampled signal |
| 6 | **Generate subpixels** | DisplayLayouts | compute | apply subpixel geometry & per-channel drive |
| 7 | **Apply layout** | DisplayLayouts | compute | mask/blend the layout footprint (fill factor, gaps) |
| 8 | **Brightness model** | Renderer | compute | gamma of emitter, fill-factor comp, edge softening |
| 9 | **Imperfections** | Artifacts | compute | mura, uniformity, dead/stuck/hot, banding, bleed |
| 10 | **Temporal** | Animation | compute | scanlines, rolling refresh, PWM, response, burn-in accum |
| 11 | **Lens** | Lens | compute | chromatic aberration, bloom/glow, defocus, reflection, moiré, curvature |
| 12 | **To output** | Color | compute | linear → output transfer/space; tone-map to avoid clip |
| 13 | **Egress** | Core | copy/download | write engine texture → host buffer |

Design notes:
- Stages 5–8 are frequently **fused** into a single compute dispatch on GPU (one kernel reads source + layout LUTs and writes the emissive result) to minimize bandwidth. The FrameGraph exposes them separately for the CPU path, testing, and clarity; the GPU executor may merge adjacent compatible stages.
- **Burn-in (stage 10)** has a temporal-accumulation aspect. Since MFR forbids frame-to-frame state, burn-in is modeled as a *closed-form function of layer time* over an assumed content-history model (see §12), not as a running accumulator. An optional user-supplied burn-in mask provides the spatial term. This keeps every frame independent.
- Every stage is a no-op passthrough when its feature group is disabled, so the graph shrinks for cheap looks (draft mode).

---

## 5. Class Model

### 5.1 Engine core

```
Engine                       facade; owns Backend + ResourceCache; render(RenderRequest)
 ├─ RenderRequest            { ImageView in, ImageView out, ParamSnapshot, TimeInfo, RenderQuality }
 ├─ RenderContext            per-render scratch; NOT shared; holds transient resources
 ├─ FrameGraph               compiled stage list for a given ParamSnapshot hash
 │   └─ Stage (interface)    setup(ctx), record(cmd, ctx), (optional) reflectResources()
 ├─ Backend (interface)      device/queue abstraction (see 5.2)
 └─ ResourceCache            keyed store of GpuBuffer/Texture/LUT/LayoutGeometry (see §9)
```

### 5.2 Backend abstraction

```
Backend (abstract)
 ├─ createTexture / createBuffer / upload / download
 ├─ createComputePipeline(ShaderId) / dispatch / barrier
 ├─ CommandList begin/end/submit
 ├─ caps() → { maxThreadgroup, hasFloat16, hasWaveOps, memoryBudget }
 └─ implementations:
     MetalBackend      (Objective-C++ .mm, MTLDevice/MTLComputePipelineState)
     D3D12Backend      (ID3D12Device, root sigs, PSO cache, descriptor heaps)
     OpenGLBackend     (GL 4.3 compute; GLES3.1 path for parity checks)
     CpuBackend        (thread-pool tiled executor; SIMD kernels)
```

`Backend` is selected at `Engine` construction by a capability probe with the priority Metal > D3D12 > OpenGL > CPU, overridable by the `GPU Enable`/`CPU Enable`/adaptive params. All backends implement the **same** `Stage` kernels behind `ShaderId` enums; shader sources are backend-specific but semantically identical and covered by the same golden tests.

### 5.3 Display model

```
IDisplayLayout (interface)
 ├─ id() → DisplayType
 ├─ buildGeometry(const LayoutParams&) → LayoutGeometry   // cacheable, deterministic
 └─ samplePlan() → SubpixelPlan                            // taps, channel routing, weights

Concrete: LcdStripeLayout (RGB/BGR), OledLayout, PentileLayout, DiamondPentileLayout,
          MiniLedLayout, MicroLedLayout, CrtShadowMaskLayout, CrtApertureGrilleLayout,
          LedBillboardLayout, GameBoyLcdLayout, NintendoDsLayout, RetinaLcdLayout,
          StudioDisplayLayout, MacBookMiniLedLayout, SamsungAmoledLayout,
          RgbLedMatrixLayout, HexLayout, CircleLayout, SquareLayout,
          RoundedSquareLayout, DiamondLayout

LayoutGeometry     { cell pitch, rotation, aspect, per-subpixel rects/SDF params,
                     fill factor, jitter field seed } — POD, hashable, cache key
DisplayLayoutRegistry  maps DisplayType → factory; presets reference by enum
```

Layouts never rasterize to a bitmap. They emit **analytic descriptions** (rects, rounded-rect corner radius, circle/hex SDF parameters, stripe pitch). The GPU kernel evaluates coverage per output pixel via signed-distance functions with analytic anti-aliasing (`smoothstep` over the pixel footprint), which is resolution-independent and free of texture memory.

### 5.4 Feature modules (all stateless; configured by ParamSnapshot)

```
Color:      TransferFunction (sRGB/P3/Rec709/Rec2020/linear), Lut1D/Lut3D builders,
            WhiteBalance, Vibrance/Saturation, HighlightCompress, ShadowLift
Sampling:   Resampler (Box/Bilinear/Bicubic/Lanczos3), footprint estimation
Noise:      Hash (PCG/xxhash-derived), ValueNoise, PerlinNoise, BlueNoiseTile
Artifacts:  DefectField (dead/stuck/hot pixels via seeded hash),
            MuraField, UniformityField, Banding, BacklightBleed, DirtyScreen,
            LightLeak, Vignette, BurnIn
Lens:       ChromaticAberration, Bloom, Glow, Defocus, Reflection, Refraction,
            Moire (analytic beat-frequency), ScreenCurvature, GlassStack (polarizer/AR)
Animation:  Scanline, RollingRefresh, Pwm, RandomFlicker, Twinkle, TemporalNoise,
            ResponseTime (LCD delay / OLED instant), RollingShutter
Math:       vec2/3/4, mat2/3/4, transforms, easing, clamp/saturate, fract, hash glue
```

Each module exposes a small `struct Params` (slice of the snapshot) and a pure `evaluate(...)` used identically by CPU code and mirrored in shader code.

---

## 6. GPU Pipeline

### 6.1 Execution model
- **Compute-shader first.** Every heavy stage is a compute dispatch over the output grid (typically one thread per output pixel, threadgroup 8×8 or 16×16 tuned per backend/`caps`).
- **One command list per render request**, built on the calling MFR worker thread, submitted, and waited on (or fenced) before egress. No cross-request command sharing.
- **Resource residency:** source uploaded once (or borrowed via shared/managed memory when the host format allows zero-copy), intermediates live in transient pool textures obtained from `RenderContext`, cached LUT/geometry buffers are read-only and shared safely across concurrent renders (immutable after build).
- **Stage fusion:** the GPU executor merges stages 4–8 into a "synthesis" kernel and 9–11 into an "imperfection+optics" kernel where parameters permit, cutting round-trips to VRAM. Fusion is a graph-compile decision, transparent to stage authors.

### 6.2 Backend specifics
- **Metal (primary):** `MTLComputePipelineState` per `ShaderId`, `MTLHeap`-backed transient textures, `MTLSharedEvent` fences. Uses `half` for emission math where precision allows; `float` for color-space conversions. Argument buffers pack the `ParamSnapshot` GPU-mirror.
- **D3D12:** root signature with a CBV for params, SRV/UAV descriptor tables for textures/LUTs; PSO cache serialized to the shader cache directory; placed resources on a transient heap; timeline fence per queue.
- **OpenGL 4.3+ (fallback):** `glDispatchCompute`, SSBOs for geometry/LUTs, image load/store for textures. GLES3.1 path exists for parity testing and low-end targets. This backend prioritizes correctness/portability over peak throughput.

### 6.3 Precision & determinism
- Working space is linear `float16` for emission, `float32` for color transforms and anywhere banding would appear (LUT sampling, gamma).
- Determinism across backends is validated by golden tests with per-channel tolerance (≤ 1 LSB at 8-bit, ≤ 2/65535 at 16-bit). Any op that can't be made bit-stable across vendors (e.g., fast-math reassociation) is pinned to a defined evaluation order in the kernel.

### 6.4 GPU loss / fallback
On device removal or pipeline-creation failure, the backend reports `Result::Error`, the `Engine` transparently retries on the next-priority backend, and finally the CPU backend — logged once, never crashing the host.

---

## 7. CPU Fallback Pipeline

### 7.1 Structure
- A **tiled, thread-pool executor.** The output image is split into tiles (e.g., 64×64); tiles are independent tasks. This mirrors GPU threadgroups and gives the same locality.
- Kernels are written once as scalar C++ that the compiler auto-vectorizes, with hand-written **SIMD** (SSE4.2/AVX2 on x86, NEON on ARM) fast paths for the hottest kernels (synthesis, chromatic aberration, bloom separable blur). SIMD is selected at runtime via CPU-feature detection; scalar is always the reference.
- The CPU path uses the **same** `evaluate()` functions as documented for the modules, so it is the semantic ground truth for golden images.

### 7.2 Parity
CPU and GPU share LUT builders and layout geometry (both generated on CPU). Only the per-pixel evaluation differs in implementation, not in math. Golden tests run identical parameter sets through both and diff.

### 7.3 When CPU runs
- No compatible GPU / GPU disabled by user.
- `CPU Enable` forced.
- GPU-loss fallback.
- Unit tests (headless CI has no GPU).

### 7.4 Shared shader/kernel authoring
To avoid three-way drift, math-heavy helpers live in `Engine/*/` headers as `constexpr`/inline C++ and are **transpiled-by-convention** into `.metal`/`.hlsl`/`.comp` — i.e., the shader sources are thin wrappers that include a generated header of shared function bodies produced by a small build-time codegen step (documented in `Documentation/GPU_PIPELINE.md`). This keeps one source of truth for e.g. the sRGB transfer function.

---

## 8. Parameter Definitions

Parameters are grouped exactly as the UI groups (§ below). Each parameter is declared once in a **parameter table** (`Host/AfterEffects/Parameters/`) as `{ id, group, type, default, min, max, ui, mfr_flags }`, and flattened into `ParamSnapshot`. The engine sees only the snapshot — it has no notion of AE param indices.

### 8.1 Parameter groups & controls (UI ↔ snapshot)

**Display** — Enable, Pixel Size, Dot Size, Spacing, Resolution Scale, Softness, Brightness Compensation, Edge Softening, Pixel Roundness, Pixel Rotation, Pixel Aspect Ratio, Grid Offset X, Grid Offset Y, Grid Rotation, Pixel Randomness.

**Pattern (Display Type)** — enum popup over all 22 layouts (LCD RGB/BGR, OLED, PenTile, Diamond, MiniLED, MicroLED, CRT Shadow Mask, CRT Aperture Grille, LED Billboard, GameBoy, DS, Retina, Studio Display, MacBook MiniLED, Samsung AMOLED, RGB LED Matrix, Hex, Circle, Square, Rounded Square, Diamond).

**Subpixels** — Enable, Size, Gap, Softness, Brightness, Gamma, RGB independent scale (R/G/B), RGB ordering (enum).

**Color** — Chromatic Aberration {Enable, Amount, Direction(Radial/H/V)}, Independent RGB Offset {R xy, G xy, B xy}, Contrast, Brightness, Exposure, Gamma, Saturation, Vibrance, White Balance, Tint, Highlight Compression, Shadow Lift, Linear Workflow (bool), Input/Output color space (enums: sRGB/P3/Rec709/Rec2020).

**Display Characteristics** — Pixel Glow {Radius, Intensity}, Bloom {Threshold}, Black Level, Backlight Bleed, Display Diffusion, Display Noise, Pixel Flicker, Pixel Aging, Pixel Response Time, Image Persistence.

**Artifacts** — Dead Pixels {Count 0–10000, Seed, Brightness, Color, Clusters}, Stuck Pixels {mode: Random RGB/Green/Blue/Red/White}, Hot Pixels, Mura, Panel Uniformity, Brightness Drift, Column Defects, Row Defects, Banding, Dirty Screen {Dust, Hair, Micro Scratches, Fingerprints, Pressure Marks}, Light Leakage, Vignetting, Burn-In {Enable, Intensity, Age, Persistence, Recovery Speed, Ghosting, Logo/Status Bar/Window/Taskbar presets, Custom Mask import}.

**Animation** — Scanlines {Thickness, Opacity, Movement}, Rolling Refresh {Direction, Speed}, Refresh Rate {24/30/60/90/120/144/165/240}, PWM {Frequency, Duty Cycle, Intensity}, Random Flicker, Pixel Twinkle, Temporal Noise, Pixel Warm-Up, LCD Response Delay, OLED Instant Mode.

**Lens / Camera Interaction** — Rolling Shutter {Enable, Readout Time, Sensor Direction, Offset, Camera Sync}, Chromatic Aberration (lens), Lens Blur, Display Reflection, Display Refraction, Moiré, Camera Defocus, Screen Curvature, Glass Thickness, Polarizer Simulation, Anti-Reflective Coating.

**Performance** — GPU Enable, CPU Enable, Adaptive Quality, Draft Mode, Preview Resolution, Final Resolution, Tile Rendering, Pattern Cache, Shader Cache, Mask Cache, Memory Usage Display, Render Statistics.

**Presets** — popup of the 20 tuned presets (Apple Studio Display … Damaged Display).

**Utilities** — Reset, Reset Current Category, Randomize, Copy/Paste Settings, Import/Export Preset, Save/Load User Preset.

### 8.2 Snapshot versioning
`ParamSnapshot` carries a `schemaVersion`. Preset (de)serialization and project reload migrate older snapshots forward. Adding a parameter bumps the version and supplies a default so old projects re-open identically.

---

## 9. Data Flow, Caching & Memory Ownership

### 9.1 Data flow (one render)
```
AE render CB ─► HostAdapter builds ImageView(in), ImageView(out), ParamSnapshot, TimeInfo
             ─► Engine.render(request)
                 ├─ ResourceCache.getOrBuild(layoutGeometry | LUTs | masks)  [keyed by param hash]
                 ├─ FrameGraph.compile(snapshot)  [cached by snapshot topology hash]
                 ├─ Backend: upload source → run stages → download result
             ◄─ writes into ImageView(out) memory (host-owned)
```

### 9.2 Memory ownership rules
- **Host owns** the AE input/output buffers. The engine receives them as `ImageView` (borrowed, non-owning). The engine never frees or resizes host memory.
- **Engine owns** all GPU resources, transient pools, and cache entries via RAII wrappers (`GpuTexture`, `GpuBuffer`, unique/shared handles). No raw `new`/`delete`; no manual `free`.
- **Cache entries** are `shared_ptr<const T>` — immutable once built, safe to read from many render threads. They are keyed by a hash of the relevant param subset and evicted by LRU under a memory budget.
- **User mask blobs** (custom burn-in) are copied into engine-owned storage at snapshot-build time so the engine never holds a pointer into host-managed memory beyond the call.

### 9.3 What is cached (regenerate only on param change)
| Cache | Key | Invalidated by |
|-------|-----|----------------|
| Layout geometry | display type + geometry params | pixel size, roundness, rotation, aspect, spacing, randomness seed |
| 1D/3D color LUTs | color params + spaces | any color-group change |
| Defect/mura/uniformity mask fields | seed + counts + resolution | artifact params, output size |
| Bloom/glow blur kernels | radius | glow/bloom radius |
| Compiled shader PSOs | ShaderId + backend | driver/shader version (persisted to disk) |
| FrameGraph topology | enabled-feature bitset | any enable toggle |

Everything above is **read-only during rendering**, which is what makes MFR safe.

---

## 10. Thread Safety & MFR Compatibility

### 10.1 The contract
After Effects MFR calls the effect on **many worker threads simultaneously**, each rendering a different frame. The plugin must declare MFR support and be genuinely re-entrant.

### 10.2 How we satisfy it
- **No global mutable state.** No file-scope non-const statics touched during render. Config lives in `ParamSnapshot` (per-render, by value).
- **No frame dependencies.** Each frame is a pure function of `(snapshot, time, source)`. Temporal effects (burn-in, flicker, rolling shutter) are **closed-form in `time`**, never accumulated across calls.
- **Immutable shared caches.** Cache reads take `shared_ptr<const T>`; builds happen under a mutex but the built object is never mutated afterward, so readers need no lock. Cache lookups use a sharded, lock-minimal map; a double-checked build guard prevents duplicate work without serializing renders.
- **Per-thread render context.** Each `render()` call allocates its own `RenderContext` (transient GPU/CPU scratch) from a pool. Backends hand out per-thread command allocators/queues; the Metal/D3D12 device is shared (thread-safe for resource creation) but command recording is per-thread.
- **Deterministic RNG.** All randomness derives from `hash(pixel_coord, seed, time_bucket)` — no stateful global generators.

### 10.3 Instance vs. sequence data
- Sequence data holds only immutable setup and a handle to the shared `Engine`/cache — flattened for MFR, never written during render.
- The `Engine` and `ResourceCache` are shared and internally synchronized; everything they expose to render threads is `const`.

---

## 11. Performance Strategy

1. **Do less work when possible:** disabled groups drop stages from the FrameGraph; Draft/adaptive quality lowers internal resolution and kernel taps; preview vs. final resolution split.
2. **Bandwidth over compute:** fuse stages, keep intermediates in `float16`, prefer on-chip threadgroup memory for separable blurs (bloom/glow/defocus).
3. **Cache aggressively:** §9 — geometry, LUTs, masks, PSOs, blur kernels built once.
4. **Tile rendering:** both GPU (threadgroup tiling) and CPU (task tiles) for cache locality and to respect AE's smart-render output rects (render only the requested region).
5. **Separable & analytic:** blurs are separable; moiré and layout coverage are analytic (SDF) rather than supersampled where math permits; supersampling is reserved for edges via analytic AA.
6. **SIMD on CPU**, wave/subgroup ops on GPU for reductions (bloom threshold, statistics).
7. **Adaptive quality** monitors per-frame cost (render statistics) and can scale taps/resolution to hold interactive rates during preview; final renders always use full quality.
8. **Memory budget:** cache honors a configurable budget (default derived from `Backend.caps().memoryBudget`); LRU eviction; memory-usage readout surfaced in the Performance group.

---

## 12. Burn-In Model (design detail worth calling out)

Because MFR forbids accumulators, burn-in is modeled as:
```
burnIn(x, t) = intensity
             * agingCurve(age)                    // how "worn" the panel is
             * spatialTerm(x)                      // from preset regions OR user mask OR
                                                   //   a cheap running-luminance proxy
             * (1 - recovery(t, recoverySpeed))    // partial healing over time
```
`spatialTerm` sources, in priority: user-imported grayscale mask → preset region masks (logo/status bar/window/taskbar, generated analytically) → an optional estimate from the current frame's own bright regions (approximation, since we lack history). Ghosting/persistence add a time-lagged copy of the signal blended by `persistence`. This is documented so reviewers understand it is *physically motivated but frame-independent by construction*.

---

## 13. Color Pipeline

- Internal working space: **linear RGB** (scene-linear), float.
- Input decode and output encode via `TransferFunction` for sRGB, Display P3, Rec.709, Rec.2020 (and pass-through linear). Primaries conversions use 3×3 matrices; optional 3D LUT path for creative grades.
- "Linear Workflow" toggle controls whether AE hands us linear or display-encoded data and is honored on both ends.
- **No clipping by default:** highlights above 1.0 are preserved through the emissive/bloom stages and tone-mapped (highlight compression) at stage 12 rather than hard-clamped, so glow/bloom read correctly. 16-bit and 32-bit float AE pipelines are supported end-to-end.

---

## 14. Presets

20 tuned presets shipped as engine-level `ParamSnapshot` blobs plus AE `.ffx`: Apple Studio Display, MacBook Pro MiniLED, Apple Retina, Samsung AMOLED, Sony OLED, Sony Trinitron, Dell IPS, Cheap TN Panel, LED Billboard, Airport Display, CRT Television, Arcade CRT, Broken LCD, Old Laptop, GameBoy, Nintendo DS, Retro LCD, Broken OLED, Burned OLED, Damaged Display. Each is authored against the layout registry + artifact params; presets are just data, versioned with the snapshot schema.

---

## 15. Testing Strategy

- **Unit tests (no AE):** math, color transfer round-trips, layout geometry determinism, hash/noise distribution, cache eviction, snapshot (de)serialization/migration.
- **Golden-image tests:** fixed params+source → compare CPU vs GPU vs stored reference within tolerance.
- **Property tests:** determinism (same input ⇒ same output), disabled-feature = passthrough, MFR (N threads rendering M frames concurrently produce identical results to serial).
- **Fuzz:** snapshot deserializer against corrupt/old data.
- CI runs the engine + tests headless (CPU backend) on Windows/macOS.

---

## 16. Build System

- **CMake** top-level with three targets: `pdengine` (static lib), `PixelDisplayPro` (AE plugin), `pdtests`.
- Platforms: Windows (x64), macOS Intel, macOS Apple Silicon (universal2). Generators: Visual Studio + Xcode; Ninja for CI.
- AE SDK located via a `cmake/FindAfterEffectsSDK.cmake` using an env/cache var; the engine target never references it.
- macOS builds `.plugin` bundle; Windows builds `.aex`. Shader compilation (metallib / DXC / glslang) wired as custom build steps producing the generated headers referenced in §7.4.
- Reproducible builds: pinned toolchain files under `cmake/`, no network at configure/build, deterministic archive stamps.

---

## 17. Future Expansion Plan

- **Premiere Pro adapter:** new `Host/Premiere/` implementing the same three-type interface (Premiere's SDK shares much with AE's PF suite). Engine untouched.
- **OpenFX adapter:** `Host/OpenFX/` mapping OFX params/clips to `ParamSnapshot`/`ImageView`. Engine untouched.
- **Vulkan backend:** add `Renderer/GPU/Vulkan/` implementing `Backend`; slots into the priority probe.
- **Additional display models:** implement `IDisplayLayout`, register, add a preset. No pipeline changes.
- **Standalone/real-time:** the engine already has no host coupling; a GLFW/OFX-less harness could drive it for real-time use.

---

## 18. Milestone Plan (each milestone compiles & tests green before the next)

| M | Deliverable | Definition of done |
|---|-------------|--------------------|
| 1 | Core architecture | Engine facade, FrameGraph, Backend iface, CPU backend skeleton, CMake, tests scaffold — builds & passes empty-pipeline passthrough test |
| 2 | Basic display renderer | Grid + Square/RoundedSquare layout, resample, linear color, emission — renders a recognizable "screen" (CPU + one GPU backend) |
| 3 | Subpixel layouts | RGB/BGR stripe, OLED, PenTile, Diamond, CRT masks, hex/circle; subpixel controls |
| 4 | Color pipeline | Full transfer/primaries, LUTs, grade controls, no-clip highlights |
| 5 | Display artifacts | Dead/stuck/hot, mura, uniformity, banding, bleed, dirty screen, vignette |
| 6 | Burn-in simulation | Closed-form burn-in + presets + custom mask import |
| 7 | Rolling shutter | Time-parameterized readout, sync, offset |
| 8 | Lens simulation | Chromatic aberration, bloom/glow, defocus, reflection/refraction, moiré, curvature, glass stack |
| 9 | GPU optimization | Stage fusion, half precision, PSO/shader cache, adaptive quality, D3D12 + GL backends at parity |
| 10 | UI polish | Collapsible groups, all controls wired, memory/statistics readouts |
| 11 | Preset system | 20 presets, import/export, user presets, copy/paste, randomize |
| 12 | Documentation | Per-subsystem docs complete, build guide, extension guide |

---

## 19. Open Questions for Approval

1. **Primary dev platform first?** Metal-first (macOS) or D3D12-first (Windows) for Milestone 2's GPU path? (CPU path is built regardless.)
2. **AE SDK availability** — is the After Effects SDK present in this environment / do you want the plugin target gated behind SDK presence so `pdengine` + tests build in CI without it?
3. **Repository intent** — should this repo hold the full production plugin, or a reference/portfolio implementation? (Affects how much backend depth vs. breadth to prioritize.)
4. **Minimum AE version** target (affects SmartFX/MFR API availability).
5. Any deviation wanted from the milestone ordering above?

---

*End of design draft. Implementation begins only after approval.*
