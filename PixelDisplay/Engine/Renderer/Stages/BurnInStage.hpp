// PixelDisplay Pro — Engine/Renderer/Stages/BurnInStage.hpp
//
// Burn-in / image-persistence / ghosting (DESIGN.md §4 step 10, §12). Worn
// regions emit less and retain a faint ghost of the burned content; the effect
// evolves with layer time but is computed closed-form (no accumulator), so it
// remains deterministic and MFR-safe.
#pragma once

#include "Engine/Core/Stage.hpp"

namespace pd::stages {

class BurnInStage final : public Stage {
public:
    const char* name() const override { return "BurnIn"; }
    ShaderId shaderId() const override { return ShaderId::Temporal; }
    void executeCpu(RenderContext& ctx) const override;
};

}  // namespace pd::stages
