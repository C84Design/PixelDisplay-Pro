// PixelDisplay Pro — Engine/Core/FrameGraph.cpp
#include "Engine/Core/FrameGraph.hpp"

#include "Engine/Renderer/Stages/BurnInStage.hpp"
#include "Engine/Renderer/Stages/ColorEncodeStage.hpp"
#include "Engine/Renderer/Stages/ImperfectionsStage.hpp"
#include "Engine/Renderer/Stages/OpticsStage.hpp"
#include "Engine/Renderer/Stages/SynthesisStage.hpp"
#include "Engine/Renderer/Stages/TemporalStage.hpp"

namespace pd {

namespace {

// Which optional stages a snapshot activates. The synthesis and color-encode
// stages are always present for a non-passthrough render; the rest are gated by
// their feature groups so the graph stays minimal for cheap looks.
struct Enabled {
    bool synthesis = false;
    bool imperfections = false;
    bool burnIn = false;
    bool temporal = false;
    bool optics = false;
    // Value bits that change stage *behaviour selection* (not just on/off).
    std::uint64_t displayType = 0;
    std::uint64_t outputSpace = 0;
    bool subpixels = false;
    bool linearWorkflow = true;
};

Enabled analyze(const ParamSnapshot& p) {
    Enabled e;
    if (p.isPassthrough()) return e;
    e.synthesis = true;
    e.displayType = static_cast<std::uint64_t>(p.displayType);
    e.subpixels = p.subpixel.enable;
    e.outputSpace = static_cast<std::uint64_t>(p.color.outputSpace);
    e.linearWorkflow = p.color.linearWorkflow;

    const ArtifactParams& a = p.artifacts;
    const DisplayCharacteristics& ch = p.characteristics;
    e.imperfections =
        a.deadPixelCount || a.stuckPixelCount || a.hotPixelCount || a.mura > 0 ||
        a.panelUniformity > 0 || a.brightnessDrift > 0 || a.columnDefects > 0 ||
        a.rowDefects > 0 || a.banding > 0 || a.dust > 0 || a.hair > 0 ||
        a.microScratches > 0 || a.fingerprints > 0 || a.pressureMarks > 0 ||
        a.lightLeakage > 0 || a.vignetting > 0 || ch.backlightBleed > 0 ||
        ch.blackLevel > 0;

    e.burnIn = p.burnIn.enable;

    const AnimationParams& an = p.animation;
    const LensParams& ln = p.lens;
    e.temporal =
        an.scanlineOpacity > 0 || an.rollingRefreshEnable || an.pwmIntensity > 0 ||
        an.randomFlicker > 0 || an.pixelTwinkle > 0 || an.temporalNoise > 0 ||
        an.pixelWarmUp > 0 || (ln.rollingShutterEnable && ln.readoutTimeMs > 0);

    e.optics =
        ch.glowIntensity > 0 || ch.bloomIntensity > 0 || ln.lensBlur > 0 ||
        ln.cameraDefocus > 0 || ln.chromaticAberration > 0 || ln.screenCurvature > 0 ||
        ln.refraction > 0 || ln.reflection > 0 || ln.moire > 0 || ln.polarizer > 0 ||
        ln.antiReflectiveCoating > 0;
    return e;
}

std::uint64_t hashEnabled(const Enabled& e) {
    std::uint64_t hash = 1469598103934665603ull;  // FNV-1a
    auto mix = [&hash](std::uint64_t v) { hash ^= v; hash *= 1099511628211ull; };
    mix(e.synthesis);
    mix(e.displayType);
    mix(e.subpixels);
    mix(e.imperfections);
    mix(e.burnIn);
    mix(e.temporal);
    mix(e.optics);
    mix(e.outputSpace);
    mix(e.linearWorkflow);
    return hash;
}

}  // namespace

// Topology key computed WITHOUT allocating stages, so the engine can cache
// compiled graphs by this key (identical topology => reuse). A compiled graph
// reads all parameter *values* live from the RenderContext, so it is valid for
// any snapshot sharing this topology.
std::uint64_t FrameGraph::topologyKey(const ParamSnapshot& params) {
    return hashEnabled(analyze(params));
}

FrameGraph FrameGraph::compile(const ParamSnapshot& params) {
    FrameGraph g;
    const Enabled e = analyze(params);
    g.topologyHash_ = hashEnabled(e);

    if (!e.synthesis) return g;  // passthrough: empty graph

    // Pipeline order (all intermediate stages operate in scene-linear):
    g.stages_.push_back(std::make_unique<stages::SynthesisStage>());   // steps 3–8
    if (e.imperfections) g.stages_.push_back(std::make_unique<stages::ImperfectionsStage>());  // 9
    if (e.burnIn)        g.stages_.push_back(std::make_unique<stages::BurnInStage>());          // 10
    if (e.temporal)      g.stages_.push_back(std::make_unique<stages::TemporalStage>());        // 10
    if (e.optics)        g.stages_.push_back(std::make_unique<stages::OpticsStage>());          // 11
    g.stages_.push_back(std::make_unique<stages::ColorEncodeStage>());  // step 12

    return g;
}

}  // namespace pd
