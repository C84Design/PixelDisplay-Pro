// PixelDisplay Pro — Engine/Core/FrameGraph.cpp
#include "Engine/Core/FrameGraph.hpp"

#include "Engine/Renderer/Stages/ColorEncodeStage.hpp"
#include "Engine/Renderer/Stages/SynthesisStage.hpp"

namespace pd {

// Milestone 1 builds an empty graph (passthrough). Subsequent milestones append
// concrete stages here as they come online:
//   M2: Synthesis        M4: ColorEncode
//   M5: Imperfections    M6: burn-in (Temporal)
//   M7: rolling shutter  M8: Optics
// The topology hash folds in which stages were added so equal configs share a
// compiled graph via the ResourceCache.
FrameGraph FrameGraph::compile(const ParamSnapshot& params) {
    FrameGraph g;
    std::uint64_t hash = 1469598103934665603ull;  // FNV-1a offset basis
    auto mix = [&hash](std::uint64_t v) {
        hash ^= v;
        hash *= 1099511628211ull;
    };

    if (params.isPassthrough()) {
        g.topologyHash_ = hash;  // empty graph
        return g;
    }

    // Milestone 2: the fused synthesis stage (resample + linear + emitter).
    mix(static_cast<std::uint64_t>(params.displayType));
    g.stages_.push_back(std::make_unique<stages::SynthesisStage>());

    mix(static_cast<std::uint64_t>(params.subpixel.enable));

    // Later milestones insert Imperfections (M5), Temporal/burn-in (M6/M7), and
    // Optics (M8) here — all operating in scene-linear.

    // Final stage: linear -> output gamut/transfer (DESIGN.md §4 step 12).
    mix(static_cast<std::uint64_t>(params.color.outputSpace));
    mix(static_cast<std::uint64_t>(params.color.linearWorkflow));
    g.stages_.push_back(std::make_unique<stages::ColorEncodeStage>());

    g.topologyHash_ = hash;
    return g;
}

}  // namespace pd
