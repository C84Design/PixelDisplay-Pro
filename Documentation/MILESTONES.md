# Milestone Status

Each milestone must compile and pass tests before the next begins
(see DESIGN.md §18). This file tracks live progress.

| M | Title | Status |
|---|-------|--------|
| 1 | Core architecture | ✅ Complete |
| 2 | Basic display renderer | ✅ Complete (CPU path) |
| 3 | Subpixel layouts | ✅ Complete |
| 4 | Color pipeline | ✅ Complete |
| 5 | Display artifacts | ✅ Complete |
| 6 | Burn-in simulation | ✅ Complete |
| 7 | Rolling shutter | ⬜ Planned |
| 8 | Lens simulation | ⬜ Planned |
| 9 | GPU optimization (Metal, then D3D12/GL) | ⬜ Planned |
| 10 | UI polish | ⬜ Planned |
| 11 | Preset system | ⬜ Planned |
| 12 | Documentation | ⬜ Planned |

## Milestone 1 — Core architecture (complete)

**Delivered**
- Two-layer boundary established: `Engine/` compiles as a standalone static
  library (`pdengine`) with zero Adobe dependency; the AE plugin target is gated
  behind `PD_AE_SDK_ROOT`.
- Public host-facing value types: `ImageView`, `ParamSnapshot` (full parameter
  set, grouped per DESIGN.md §8, versioned), `TimeInfo`, `RenderRequest`.
- Core engine: `Engine` facade, `Backend` abstraction with priority-based
  selection (Metal > D3D12 > OpenGL > CPU) and fallback, `FrameGraph` compiler,
  `Stage` interface, per-render `RenderContext` with ping-pong `WorkingImage`.
- Always-available `CpuBackend` with a dependency-free tiled `ThreadPool`.
- Format-aware pixel I/O (ARGB/RGBA × 8/16/32F).
- Dependency-free test harness + Milestone 1 acceptance tests.

**Verification**
- `pdtests`: 5/5 passing (Release).
- Empty-pipeline passthrough is bit-exact across ARGB8, RGBA16, ARGB16, RGBA32F.
- MFR contract exercised: 8 threads × 20 iterations produce identical,
  source-exact output.
- Clean under AddressSanitizer + UndefinedBehaviorSanitizer + leak detection.
- ThreadSanitizer used to validate the thread-pool synchronization.

**Notes**
- Temporal effects will be closed-form in time (no accumulators) so MFR safety
  is preserved — see DESIGN.md §10, §12.

## Milestone 6 — Burn-in simulation (complete)

**Delivered** (`BurnInStage` + `Artifacts/BurnInModel.hpp`)
- Closed-form burn-in `intensity·aging(age)·wear(x)·growth(t)·(1−recovery)` —
  no accumulator, so every frame is independent and MFR-safe (DESIGN.md §12).
- Wear map sources with priority: imported grayscale mask → analytic preset
  regions (logo / status bar / window / taskbar) → content-luminance proxy.
- Differential-aging dimming of worn emitters + a faint persistent ghost
  (ghosting control) visible even on black content.
- Image persistence / retention trail. Effect intensifies with layer time.

**Verification**
- `pdtests`: 28/28. Confirms a status-bar ghost on black, intensification over
  time, custom-mask spatial control, and closed-form determinism (equal time =>
  identical output across 8 threads).
- Clean under ASan + UBSan.

## Milestone 5 — Display artifacts (complete)

**Delivered** (`ImperfectionsStage`, in linear light between synthesis and encode)
- Per-cell defects aligned to the display grid via shared `GridMapping`:
  dead pixels (count/seed/brightness/colour/clusters), stuck pixels (Random RGB
  / R / G / B / White), hot pixels.
- Panel-scale non-uniformity: mura, panel uniformity, brightness drift, column
  and row defects (all fbm/hash-driven, deterministic).
- Banding (limited effective bit depth), backlight bleed, black level.
- Dirty screen: dust specks, fingerprint smudge, micro-scratches, hair,
  pressure marks.
- Light leakage (warm corner leak) and vignetting.
- Value noise + fBm added (`Noise/Noise.hpp`); shared grid math (`Grid.hpp`).
- Stage appended only when at least one artifact is active (graph stays minimal).

**Verification**
- `pdtests`: 24/24. Covers corner vignette darkening, dead-cell creation,
  black-level floor, banding level collapse, and cross-thread determinism with a
  full artifact stack.
- Clean under ASan + UBSan.
- Visual montage of 6 artifact groups inspected — reads as genuine panel defects.

## Milestone 4 — Color pipeline (complete)

**Delivered**
- Scene-linear working space made explicit: synthesis now emits **linear** and a
  dedicated final `ColorEncodeStage` performs the only linear→display encode, so
  every downstream stage (M5 artifacts, M8 optics) operates in linear light.
- Transfer functions (`Color/Transfer.hpp`): sRGB / Rec.709 / linear with
  HDR-preserving extrapolation outside [0,1].
- Primaries conversion (`Color/Primaries.hpp`): sRGB/Rec.709, Display P3,
  Rec.2020 via D65 XYZ matrices; input→working and working→output products.
- Colour grade (`Color/Grade.hpp`): exposure, white balance, tint, contrast
  (linear pivot), shadow lift, highlight compression (no-clip rolloff),
  saturation, vibrance, brightness, gamma — all in linear light.
- Chromatic aberration + independent per-channel RGB sample offsets (radial /
  horizontal / vertical), evaluated at source-sampling time.
- Linear-workflow toggle honored end to end.

**Verification**
- `pdtests`: 19/19. Covers transfer round-trips, P3 primaries round-trip,
  exposure/saturation grade, highlight-compression no-clip, chromatic-aberration
  channel separation at an edge, and output-gamut differences.
- Clean under ASan + UBSan.

## Milestone 3 — Subpixel layouts (complete)

**Delivered**
- Subpixel decomposition in `LayoutModel`: each cell splits into channel-specific
  emissive elements, each driven only by its input channel — the core of real
  display simulation.
- Layouts: RGB & BGR vertical stripe (LCD/OLED/MiniLED/MicroLED/Retina/Studio/
  MacBook/DS), continuous aperture-grille stripes (Trinitron), RGB-dot triads
  with per-row hex offset (CRT shadow mask), spaced RGB dots (LED billboard /
  RGB matrix), PenTile RGBG, and Diamond PenTile (Samsung AMOLED).
- Subpixel controls wired: enable, size, gap, softness, brightness, per-channel
  gamma, independent R/G/B scale, RGB/BGR ordering.
- Geometric single-emitter types (Square/Circle/Hex/Diamond/GameBoy) fall back
  to the whole-pixel path when subpixels are off.

**Verification**
- `pdtests`: 13/13. Property tests confirm R/G/B land in the correct cell thirds,
  BGR reverses order, a green source lights only green subpixels, and disabling
  subpixels yields a neutral full-colour emitter.
- Clean under ASan + UBSan.
- Visual montage of 8 subpixel layouts inspected — matches real panel structure.

## Milestone 2 — Basic display renderer (complete, CPU path)

**Delivered**
- `SynthesisStage`: fuses pipeline steps 3–8 (resample → linear → emitter
  footprint → brightness) into one kernel, per DESIGN.md §4/§6.1.
- Analytic emitter footprints via SDFs (`Engine/Math/Sdf.hpp`) with resolution-
  independent anti-aliasing — Square, RoundedSquare, Circular, Diamond,
  Hexagonal (never bitmaps).
- Scene-linear working space with sRGB transfer (`Engine/Color/Transfer.hpp`);
  bilinear source resampling (`Engine/Sampling/Resampler.hpp`).
- Deterministic per-cell jitter via coordinate hashing (`Engine/Noise/Hash.hpp`)
  — no stateful RNG, so it stays MFR-safe.
- Grid controls wired: pixel size, dot size, spacing, roundness, aspect,
  grid/pixel rotation, grid offset, resolution scale, randomness, brightness
  compensation, softness, edge softening.

**Verification**
- `pdtests`: 9/9 passing. Property tests cover: effect actually applied, gaps
  darker than dot centers, full-coverage solid-colour round-trip within 3/255,
  and deterministic/MFR-safe output across 8 concurrent threads.
- Visual montage rendered and inspected (6 layout types over a gradient +
  test bars) — confirms a genuine display look, not a halftone overlay.

**Scheduling note (GPU backend)**
- The approved plan places GPU optimization in Milestone 9, and this CI/build
  environment (Linux) has no Metal/D3D12 toolchain to compile or verify GPU
  code against. To honor "each milestone must compile" and "never leave
  partially implemented systems", the GPU backends (Metal-first) are
  implemented and validated in M9 rather than committing untested, unbuildable
  GPU sources now. The backend abstraction, priority selection, and CMake
  gating are already in place so M9 slots in without engine changes.
