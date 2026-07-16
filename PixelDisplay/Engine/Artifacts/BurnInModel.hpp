// PixelDisplay Pro — Engine/Artifacts/BurnInModel.hpp
//
// Burn-in "wear map" construction (DESIGN.md §12). Burn-in is modeled as a
// closed-form function of layer time — never a frame-to-frame accumulator — so
// every frame is independent and Multi-Frame-Rendering-safe.
//
//   burn(x, t) = intensity · aging(age) · wear(x) · growth(t) · (1 − recovery)
//
// wear(x) sources, in priority: an imported grayscale mask → analytic preset
// regions (logo / status bar / window / taskbar) → a content-luminance proxy.
#pragma once

#include <algorithm>
#include <cmath>

#include "Engine/Core/ParamSnapshot.hpp"
#include "Engine/Math/Vec.hpp"

namespace pd::artifacts {

using math::clampf;
using math::smoothstepf;

/// Sample an imported grayscale wear mask at normalized coords ([-0.5,0.5]).
inline float sampleWearMask(const BurnInParams& b, float nx, float ny) {
    if (b.customMask.empty() || b.maskW <= 0 || b.maskH <= 0) return -1.0f;  // absent
    float u = clampf(nx + 0.5f, 0.0f, 1.0f);
    float v = clampf(ny + 0.5f, 0.0f, 1.0f);
    int x = std::min(static_cast<int>(u * b.maskW), b.maskW - 1);
    int y = std::min(static_cast<int>(v * b.maskH), b.maskH - 1);
    return b.customMask[static_cast<std::size_t>(y) * b.maskW + x];
}

/// Analytic wear from the enabled preset UI regions. Returns 0 if none enabled.
inline float presetRegions(const BurnInParams& b, float nx, float ny) {
    float w = 0.0f;
    auto band = [](float v, float lo, float hi) {
        // 1 inside [lo,hi] with soft 0.02 edges.
        return smoothstepf(lo - 0.02f, lo + 0.02f, v) *
               (1.0f - smoothstepf(hi - 0.02f, hi + 0.02f, v));
    };
    if (b.statusBar) w = std::max(w, band(ny, -0.50f, -0.42f));                 // top strip
    if (b.taskbar)   w = std::max(w, band(ny, 0.42f, 0.50f));                   // bottom strip
    if (b.logo)      w = std::max(w, band(nx, 0.30f, 0.45f) * band(ny, -0.45f, -0.30f));
    if (b.window)    w = std::max(w, band(nx, -0.32f, 0.32f) * band(ny, -0.28f, 0.28f) * 0.8f);
    return w;
}

/// Temporal growth: burn-in develops as the layer plays. age provides a
/// baseline so a still frame still shows wear; time strengthens it.
inline float growth(float age, double t) {
    float baseline = 0.3f * clampf(age, 0.0f, 1.0f);
    float overTime = 1.0f - std::exp(-static_cast<float>(std::max(t, 0.0)) * 0.15f);
    return clampf(baseline + overTime * (1.0f - baseline), 0.0f, 1.0f);
}

}  // namespace pd::artifacts
