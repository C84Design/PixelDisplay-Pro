# Milestone Status

Each milestone must compile and pass tests before the next begins
(see DESIGN.md §18). This file tracks live progress.

| M | Title | Status |
|---|-------|--------|
| 1 | Core architecture | ✅ Complete |
| 2 | Basic display renderer | ✅ Complete (CPU path) |
| 3 | Subpixel layouts | ⬜ Planned |
| 4 | Color pipeline | ⬜ Planned |
| 5 | Display artifacts | ⬜ Planned |
| 6 | Burn-in simulation | ⬜ Planned |
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
