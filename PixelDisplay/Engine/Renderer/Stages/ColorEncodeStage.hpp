// PixelDisplay Pro — Engine/Renderer/Stages/ColorEncodeStage.hpp
//
// Final pipeline stage (DESIGN.md §4 step 12): converts the scene-linear working
// image (Rec.709 primaries) into the requested output gamut and transfer. This
// is the ONLY place linear->display encoding happens, so every intermediate
// stage is free to work in linear light.
#pragma once

#include "Engine/Core/Stage.hpp"

namespace pd::stages {

class ColorEncodeStage final : public Stage {
public:
    const char* name() const override { return "ColorEncode"; }
    ShaderId shaderId() const override { return ShaderId::ColorEncode; }
    void executeCpu(RenderContext& ctx) const override;
};

}  // namespace pd::stages
