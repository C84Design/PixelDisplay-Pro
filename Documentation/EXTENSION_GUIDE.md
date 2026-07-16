# Extension Guide

How to extend PixelDisplay Pro without touching unrelated code. The two-layer
design keeps each of these local.

## Add a display layout

1. Add the enum value to `DisplayType` (`Engine/Core/ParamSnapshot.hpp`).
2. If it has RGB subpixels, extend `buildSubpixels()` in
   `Engine/DisplayLayouts/LayoutModel.hpp`; if it's a single geometric emitter,
   extend `emitterCoverage()` and `hasSubpixelStructure()`.
3. Add its label to the Pattern popup in
   `Host/AfterEffects/Parameters/ParameterCatalog.cpp`.
4. (Optional) add a tuned preset (below) and a golden test case.

No pipeline or backend changes required.

## Add a rendering stage

1. Create `Engine/Renderer/Stages/MyStage.{hpp,cpp}` implementing `Stage`
   (`name()`, `shaderId()`, `executeCpu(RenderContext&)`), operating in
   scene-linear.
2. Register it in `FrameGraph::compile()` (`Engine/Core/FrameGraph.cpp`) at the
   right pipeline position, gated by its feature flags, and fold those flags into
   `analyze()` / `hashEnabled()` so the ResourceCache keys correctly.
3. Add its parameters to `ParamSnapshot` and the `ParameterCatalog`.
4. Add a `ShaderId` and a matching GPU kernel for each backend (mirror the CPU
   math); add a golden test.

## Add a GPU backend

1. Implement `Backend` (`Engine/Renderer/Backend.hpp`) under
   `Engine/Renderer/GPU/<API>/`, plus a `create<API>Backend()` factory returning
   `nullptr` when unavailable.
2. Add it to the priority probe in `Engine.cpp` (`tryCreateGpuBackend`) behind a
   `PD_WITH_<API>` define and wire the sources in `Engine/CMakeLists.txt`.
3. Port the kernels (mirror `Engine/**` math) and make the golden-image parity
   suite pass before the factory returns a usable backend.

## Add a preset

Add a `PresetId`, its name in `presetName()`, and its `ParamSnapshot` in
`makePreset()` (`Host/AfterEffects/Presets/Presets.cpp`). Presets are pure data;
they serialize automatically via the shared field visitor.

## Add a new host (Premiere / OpenFX)

Create `Host/<Host>/` implementing the same three-type contract the AE adapter
uses — build a `ParamSnapshot`, wrap the host buffers as `ImageView`s, and call
`Engine::render`. Reuse `pdhostcore` (catalog + presets). The engine is untouched
(DESIGN.md §17).

## Add a parameter

1. Add the field to the relevant group struct in `ParamSnapshot`, with a default
   that preserves existing looks; bump `kSchemaVersion` if needed.
2. Add it to `ParameterCatalog` (UI) and to `fieldsOf()` in `Presets.cpp`
   (serialization) — the field visitor keeps export/import symmetric.
3. Consume it in the owning stage.
