// PixelDisplay Pro — Engine/Renderer/Stages/OpticsStage.hpp
//
// Lens / camera optics (DESIGN.md §4 step 11): screen curvature, lens chromatic
// aberration, refraction/glass, bloom, pixel glow, lens blur, camera defocus,
// moiré, reflection, polarizer and anti-reflective coating. Operates in
// scene-linear so bright emitters bloom correctly, before the final encode.
#pragma once

#include "Engine/Core/Stage.hpp"

namespace pd::stages {

class OpticsStage final : public Stage {
public:
    const char* name() const override { return "Optics"; }
    ShaderId shaderId() const override { return ShaderId::Optics; }
    void executeCpu(RenderContext& ctx) const override;
};

}  // namespace pd::stages
