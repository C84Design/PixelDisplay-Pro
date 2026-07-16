# After Effects Integration

The After Effects adapter is Layer 1: the only code that includes the Adobe SDK.
It contains **no rendering logic** — it translates AE's parameter/render
callbacks into engine calls (`Host/AfterEffects/EntryPoint/PixelDisplayPro.cpp`).

## Build gating

The plugin target configures only when `PD_AE_SDK_ROOT` points at a valid SDK
(see [BUILD.md](BUILD.md)). `pdengine` and `pdhostcore` build without it, so CI
and engine development never need the Adobe headers.

## Command dispatch

| PF_Cmd | Handler | Role |
|--------|---------|------|
| `GLOBAL_SETUP` | `GlobalSetup` | Advertise SmartFX + **Multi-Frame Rendering** (`PF_OutFlag2_SUPPORTS_THREADED_RENDERING`); create the shared `Engine`. |
| `PARAMS_SETUP` | `ParamsSetup` | Register every control by walking the shared `ParameterCatalog` (UI order = catalog order). |
| `SMART_RENDER` | `SmartRender` | Wrap input/output worlds as `ImageView`s, build a `ParamSnapshot`, call `Engine::render`. |

## MFR safety

AE calls `SmartRender` on many worker threads at once. This is safe because:

- The `Engine` and its `ResourceCache` are shared but expose only immutable,
  `const` data to render threads.
- Every render allocates its own `RenderContext` (per-call scratch).
- Temporal effects are closed-form in `TimeInfo` — no frame-to-frame state.

See DESIGN.md §10 for the full contract; the engine's MFR behaviour is verified
by the concurrency tests in `pdtests`.

## Buffer & colour bridging

`wrapWorld()` maps AE 8/16/32-bit worlds to `ARGB8/ARGB16/ARGB32F` `ImageView`s;
the engine reads them directly (`Engine/Core/PixelAccess.hpp`). Note AE's 16-bit
world is `0..0x8000`; the adapter is the place to account for that when presenting
the buffer (the engine treats `*16` as full-range `0..65535`).

## Deep-color / float

The plugin advertises `PF_OutFlag_DEEP_COLOR_AWARE` and
`PF_OutFlag2_FLOAT_COLOR_AWARE`; the engine's linear working space and no-clip
highlight handling (DESIGN.md §13) carry 16- and 32-bit pipelines end to end.
