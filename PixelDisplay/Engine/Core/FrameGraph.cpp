// PixelDisplay Pro — Engine/Core/FrameGraph.cpp
#include "Engine/Core/FrameGraph.hpp"

#include "Engine/Renderer/Stages/BurnInStage.hpp"
#include "Engine/Renderer/Stages/ColorEncodeStage.hpp"
#include "Engine/Renderer/Stages/ImperfectionsStage.hpp"
#include "Engine/Renderer/Stages/SynthesisStage.hpp"
#include "Engine/Renderer/Stages/TemporalStage.hpp"

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

    // Imperfections (M5): appended only when at least one artifact is active, so
    // the graph stays minimal for clean looks.
    const ArtifactParams& a = params.artifacts;
    const DisplayCharacteristics& ch = params.characteristics;
    const bool anyImperfection =
        a.deadPixelCount || a.stuckPixelCount || a.hotPixelCount || a.mura > 0 ||
        a.panelUniformity > 0 || a.brightnessDrift > 0 || a.columnDefects > 0 ||
        a.rowDefects > 0 || a.banding > 0 || a.dust > 0 || a.hair > 0 ||
        a.microScratches > 0 || a.fingerprints > 0 || a.pressureMarks > 0 ||
        a.lightLeakage > 0 || a.vignetting > 0 || ch.backlightBleed > 0 ||
        ch.blackLevel > 0;
    if (anyImperfection) {
        mix(0x9151u);
        g.stages_.push_back(std::make_unique<stages::ImperfectionsStage>());
    }

    // Burn-in (M6): closed-form in time, so still MFR-safe.
    if (params.burnIn.enable) {
        mix(0xB021u);
        g.stages_.push_back(std::make_unique<stages::BurnInStage>());
    }

    // Temporal / animation + camera rolling shutter (M7). All closed-form in
    // frame time/index, so MFR-safe.
    const AnimationParams& an = params.animation;
    const LensParams& ln = params.lens;
    const bool anyTemporal =
        an.scanlineOpacity > 0 || an.rollingRefreshEnable || an.pwmIntensity > 0 ||
        an.randomFlicker > 0 || an.pixelTwinkle > 0 || an.temporalNoise > 0 ||
        an.pixelWarmUp > 0 || (ln.rollingShutterEnable && ln.readoutTimeMs > 0);
    if (anyTemporal) {
        mix(0x7E3Fu);
        g.stages_.push_back(std::make_unique<stages::TemporalStage>());
    }

    // Milestone 8 inserts Optics here — operating in scene-linear.

    // Final stage: linear -> output gamut/transfer (DESIGN.md §4 step 12).
    mix(static_cast<std::uint64_t>(params.color.outputSpace));
    mix(static_cast<std::uint64_t>(params.color.linearWorkflow));
    g.stages_.push_back(std::make_unique<stages::ColorEncodeStage>());

    g.topologyHash_ = hash;
    return g;
}

}  // namespace pd
