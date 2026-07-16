// PixelDisplay Pro — Engine/Renderer/Stages/ImperfectionsStage.hpp
//
// Panel imperfections (DESIGN.md §4 step 9): the electronic and physical flaws
// that make a real panel look real — dead/stuck/hot pixels, mura, panel
// non-uniformity, brightness drift, column/row defects, banding, backlight
// bleed, black level, dirty-screen marks, light leakage, and vignetting.
//
// Operates in scene-linear light, after synthesis and before color encode. All
// randomness is coordinate-derived (deterministic, MFR-safe).
#pragma once

#include "Engine/Core/Stage.hpp"

namespace pd::stages {

class ImperfectionsStage final : public Stage {
public:
    const char* name() const override { return "Imperfections"; }
    ShaderId shaderId() const override { return ShaderId::Imperfections; }
    void executeCpu(RenderContext& ctx) const override;
};

}  // namespace pd::stages
