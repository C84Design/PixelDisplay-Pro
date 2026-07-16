# CPU Pipeline

The CPU backend (`Engine/Renderer/CPU/CpuBackend`) is the always-available
**reference** implementation and the source of truth for correctness. Every GPU
backend must match it within tolerance (see [GPU_PIPELINE.md](GPU_PIPELINE.md)).

## Flow

```
Engine::render(request)
  → ResourceCache::graph(params)      // compiled once per topology, shared
  → CpuBackend::execute(graph, req, ctx)
       decode input region → RenderContext.front()   (float RGBA)
       for each stage: stage.executeCpu(ctx); ctx.swap()   // ping-pong
       encode RenderContext.front() → output region
```

Stages read `ctx.front()` and write `ctx.back()`; the executor swaps after each,
so stage *N*'s output is stage *N+1*'s input. Intermediate buffers are
scene-linear float RGBA; only the final `ColorEncodeStage` converts to the output
transfer/gamut.

## Stage order (non-passthrough)

1. `SynthesisStage` — fuses resample → linear → emitter footprint → brightness
2. `ImperfectionsStage` — dead/stuck/hot pixels, mura, uniformity, banding,
   backlight bleed, dirty screen, vignette *(only if any artifact active)*
3. `BurnInStage` — closed-form burn-in *(if enabled)*
4. `TemporalStage` — scanlines, PWM, rolling shutter, flicker, twinkle *(if any)*
5. `OpticsStage` — bloom, glow, lens blur, defocus, curvature, CA, moiré *(if any)*
6. `ColorEncodeStage` — linear → output gamut + transfer

The frame graph appends only the stages a configuration needs, so clean looks
run a short pipeline.

## Parallelism & tiling

`ThreadPool` runs image rows/tiles across `min(hw_threads)` workers. `parallelFor`
enqueues exactly one drain-task per worker and waits until **all tasks return**
(not merely until the item counter empties) — this, plus notifying under the
completion mutex, makes the stack-local synchronization primitives safe to
destroy when the call returns (verified with ThreadSanitizer).

## SIMD

Kernels are written as auto-vectorizable scalar C++ (the reference). Hand-written
SSE4.2/AVX2/NEON fast paths for the hottest kernels (synthesis, separable blur,
chromatic aberration) are selected at runtime by CPU-feature detection; the
scalar path always remains as the correctness reference. *(SIMD fast paths are a
scheduled optimization; the scalar reference is complete and shipping.)*

## Determinism

All randomness derives from `hash(coordinate, seed[, frameIndex])`
(`Engine/Noise/Hash.hpp`) — never a stateful RNG — so results are identical
across runs, threads, and (by mirroring) GPU backends.
