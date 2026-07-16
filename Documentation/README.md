# PixelDisplay Pro — Documentation Index

The required documentation topics and where each lives:

| Topic | Document |
|-------|----------|
| Architecture (two layers, data flow, class model) | [DESIGN.md](DESIGN.md) §2–§5 |
| Rendering pipeline (13 stages, frame graph) | [DESIGN.md](DESIGN.md) §4, [CPU_PIPELINE.md](CPU_PIPELINE.md) |
| GPU pipeline (Metal/D3D12/OpenGL) | [GPU_PIPELINE.md](GPU_PIPELINE.md) |
| CPU fallback pipeline | [CPU_PIPELINE.md](CPU_PIPELINE.md) |
| Display simulation (layouts, subpixels, SDF math) | [DISPLAY_SIMULATION.md](DISPLAY_SIMULATION.md) |
| Math | [DISPLAY_SIMULATION.md](DISPLAY_SIMULATION.md) §Math, code in `Engine/Math/` |
| Artifacts & burn-in | [ARTIFACTS.md](ARTIFACTS.md) |
| Thread safety & MFR | [DESIGN.md](DESIGN.md) §10 |
| Memory management & ownership | [DESIGN.md](DESIGN.md) §9 |
| Performance strategy | [DESIGN.md](DESIGN.md) §11 |
| Color pipeline | [DESIGN.md](DESIGN.md) §13, code in `Engine/Color/` |
| After Effects integration | [AE_INTEGRATION.md](AE_INTEGRATION.md) |
| Build instructions | [BUILD.md](BUILD.md) |
| Future extension guide | [EXTENSION_GUIDE.md](EXTENSION_GUIDE.md) |
| Milestone status & verification | [MILESTONES.md](MILESTONES.md) |

## Project layout

```
PixelDisplay/
  Engine/     Layer 2 — standalone rendering engine (no Adobe deps)  → pdengine
  Host/       Layer 1 — AE host adapter; SDK-independent core         → pdhostcore
              + SDK-gated AE plugin target                            → PixelDisplayPro
  Tests/      headless engine + host tests                            → pdtests
  Documentation/
  cmake/
```

## Verification snapshot

The engine and host-core build on Windows/macOS/Linux with no Adobe SDK. The test
suite (`pdtests`, 45 cases) runs headless on the CPU reference and is clean under
AddressSanitizer + UndefinedBehaviorSanitizer and ThreadSanitizer. The GPU
backends build only on macOS/Windows and are gated behind a golden-image parity
pass (see [GPU_PIPELINE.md](GPU_PIPELINE.md)).
