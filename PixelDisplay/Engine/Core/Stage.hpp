// PixelDisplay Pro — Engine/Core/Stage.hpp
//
// A rendering stage is a pure transformation of the working image, configured
// entirely by the ParamSnapshot in the RenderContext. Stages hold no mutable
// state and are safe to reuse across concurrent renders (they are effectively
// stateless function objects; any per-render scratch lives in RenderContext).
//
// Each concrete stage implements a CPU path now; GPU backends record an
// equivalent compute dispatch keyed by the same ShaderId in later milestones.
#pragma once

#include "Engine/Core/RenderContext.hpp"

namespace pd {

/// Identifies the kernel a stage runs, so GPU backends can map to a compiled
/// pipeline state object. CPU stages ignore it beyond diagnostics.
enum class ShaderId {
    None,
    Synthesis,        // resample + linear + pixel/subpixel/layout + brightness (fused)
    Imperfections,    // artifacts + panel non-uniformity
    Temporal,         // scanlines / rolling refresh / PWM / response / burn-in
    Optics,           // chromatic aberration / bloom / glow / defocus / reflection / moiré
    ColorEncode,      // linear -> output transfer/space
};

class Stage {
public:
    virtual ~Stage() = default;

    virtual const char* name() const = 0;
    virtual ShaderId shaderId() const { return ShaderId::None; }

    /// Transform ctx.front() -> ctx.back(); implementations call ctx.swap()
    /// themselves is NOT required — the executor swaps after each stage.
    virtual void executeCpu(RenderContext& ctx) const = 0;
};

}  // namespace pd
