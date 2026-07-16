# PixelDisplay Pro

A commercial-grade Adobe After Effects plugin that **physically re-synthesizes**
an image as if it were reproduced by real display hardware — rebuilding every
pixel procedurally at the subpixel level, including realistic optical and
electronic imperfections and camera interactions.

> This is **not** a halftone, pixelation, or dot-overlay effect. It recreates the
> behavior of actual display panels (LCD, OLED, PenTile, MiniLED, MicroLED, CRT,
> LED billboards, and more), driven by a standalone GPU/CPU rendering engine.

## Status

**Design phase.** No implementation has begun yet. The complete software design
document is under review:

- 📄 [`Documentation/DESIGN.md`](Documentation/DESIGN.md) — architecture, rendering
  pipeline, folder structure, class model, GPU/CPU pipelines, parameters, data
  flow, memory ownership, thread-safety & MFR, performance strategy, milestones,
  and the future-expansion plan.

Implementation follows the milestone plan in the design document, and only after
the design is approved.

## Architecture at a glance

- **Layer 1 — Host Adapter** (`Host/AfterEffects/`): AE SDK, parameters, UI,
  presets, render callbacks, buffer exchange. No rendering logic.
- **Layer 2 — PixelDisplay Engine** (`Engine/`): standalone C++20 rendering engine
  (display simulation, subpixel layouts, artifacts, lens, GPU + CPU paths). Zero
  Adobe dependency — reusable for Premiere Pro and OpenFX via new adapters.

## Building

CMake-based; targets Windows (x64), macOS Intel, and macOS Apple Silicon. See the
build section of the design document. (Build files land with Milestone 1.)
