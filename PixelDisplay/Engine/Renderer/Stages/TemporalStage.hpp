// PixelDisplay Pro — Engine/Renderer/Stages/TemporalStage.hpp
//
// Temporal / animation effects and camera rolling shutter (DESIGN.md §4 step 10
// + Camera Interaction). Scanlines, rolling refresh bar, PWM brightness
// flicker, random flicker, pixel twinkle, temporal noise, warm-up — and a
// camera rolling-shutter model that captures each sensor row at a slightly
// different time, freezing PWM/refresh bands into horizontal banding.
//
// Everything is a closed-form function of frame time / index (no inter-frame
// state), so the effect is deterministic and Multi-Frame-Rendering-safe.
#pragma once

#include "Engine/Core/Stage.hpp"

namespace pd::stages {

class TemporalStage final : public Stage {
public:
    const char* name() const override { return "Temporal"; }
    ShaderId shaderId() const override { return ShaderId::Temporal; }
    void executeCpu(RenderContext& ctx) const override;
};

}  // namespace pd::stages
