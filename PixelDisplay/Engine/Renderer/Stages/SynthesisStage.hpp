// PixelDisplay Pro — Engine/Renderer/Stages/SynthesisStage.hpp
//
// The core display-synthesis stage (pipeline steps 3–8 in DESIGN.md §4, fused).
// For each output pixel it: locates the virtual display cell, resamples the
// source signal at the cell centroid, converts to linear, evaluates the emitter
// footprint analytically, and writes the emitted (re-encoded) colour.
//
// CPU implementation now; the Metal/D3D12/GL backends will record an equivalent
// compute kernel keyed by ShaderId::Synthesis (Milestone 9).
#pragma once

#include "Engine/Core/Stage.hpp"

namespace pd::stages {

class SynthesisStage final : public Stage {
public:
    const char* name() const override { return "Synthesis"; }
    ShaderId shaderId() const override { return ShaderId::Synthesis; }
    void executeCpu(RenderContext& ctx) const override;
};

}  // namespace pd::stages
