# GPU Pipeline

> **Build/verification status.** The GPU backends are written to mirror the CPU
> reference engine but are **compiled and validated only on macOS (Metal) and
> Windows (D3D12)**. They are **not built or run in this Linux CI environment**,
> which has no Metal/D3D12/GL toolchain. Treat the GPU sources as reviewed-but-
> unverified until a macOS/Windows build runs the golden-image parity suite
> (§"Parity" below). The CPU backend remains the always-available reference and
> the source of truth for correctness.

## Priority & selection

Backends are probed in the order **Metal → D3D12 → OpenGL → CPU** (DESIGN.md
§5.2). Each factory returns `nullptr` (or `available()==false`) when its
platform/device is missing, so `BackendPreference::Auto` falls through cleanly
to the CPU reference. `ForceCpu` / `ForceGpu` override the probe.

## Execution model

Every heavy stage is a **compute dispatch** over the output grid (one thread per
output pixel, threadgroup 8×8 or 16×16 tuned per device). One command
buffer/list is built per render request on the calling MFR worker thread and
submitted independently — no cross-request state, so concurrency is safe.

The graph executor runs the same ordered stages as the CPU path
(synthesis → imperfections → burn-in → temporal → optics → encode), ping-ponging
between two transient linear textures. Adjacent compatible stages may be **fused**
into one kernel (synthesis fuses steps 3–8) to cut VRAM round-trips; fusion is a
graph-compile decision invisible to stage authors.

## Single source of truth for math (DESIGN.md §7.4)

The colour transfer functions, SDFs, hashing, layout math, and grade operators
live once as `inline`/`constexpr` C++ in `Engine/**`. The shader sources mirror
them **function-for-function**. To keep them from drifting:

- Shared scalar helpers are kept tiny and pure so the translation is mechanical.
- The golden-image parity tests (below) fail loudly if a GPU kernel diverges
  from the CPU reference beyond tolerance, catching any drift at CI time on the
  platforms that build GPU.

A future improvement is a small codegen step that emits `.metal`/`.hlsl`/`.comp`
headers directly from the shared C++ bodies; until then the mirror is manual and
guarded by parity tests.

## Precision & determinism

- Emission math in `half` where precision allows; colour-space conversions and
  LUT sampling in `float32` to avoid banding.
- Any operation that could reassociate under fast-math is pinned to a defined
  evaluation order so results are stable across vendors.

## Caching (ResourceCache)

Compiled graphs are already memoized by topology key (`ResourceCache`, M9, CPU-
verified). The same cache will hold, keyed by the relevant parameter subset:
compiled pipeline-state objects (persisted to the shader cache dir), procedural
layout geometry buffers, colour LUTs, blur kernels, and defect-mask textures —
all immutable after build and shared read-only across concurrent renders.

## GPU loss / fallback

On device removal or pipeline-creation failure a backend returns
`ErrorCode::DeviceLost`/`BackendUnavailable`; the engine retries on the next
backend and finally the CPU reference — logged once, never crashing the host.

## Parity (required before trusting a GPU backend)

A golden-image test renders a fixed parameter matrix through the CPU reference
and each GPU backend and diffs within tolerance (≤1 LSB @ 8-bit, ≤2/65535 @
16-bit). This suite must be run on macOS/Windows; it is skipped in the Linux CI
that lacks GPU. Until it passes on a target platform, that platform ships with
the CPU backend.

## Files

```
Engine/Renderer/GPU/
  Metal/   MetalBackend.hpp/.mm     — primary; MTLDevice/MTLComputePipelineState
  D3D12/   D3D12Backend.hpp/.cpp    — root sigs, PSO cache, descriptor heaps
  OpenGL/  OpenGLBackend.hpp/.cpp   — GL 4.3 / GLES 3.1 compute
  Shaders/ PixelDisplay.metal ...   — compute kernels mirroring the CPU stages
```
