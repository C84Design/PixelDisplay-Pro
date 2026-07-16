# Milestone Status

Each milestone must compile and pass tests before the next begins
(see DESIGN.md §18). This file tracks live progress.

| M | Title | Status |
|---|-------|--------|
| 1 | Core architecture | ✅ Complete |
| 2 | Basic display renderer | ⏳ Next |
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
- GPU backends are scaffolded and gated in CMake; the Metal backend lands with
  the Milestone 2 render path per the approved plan.
