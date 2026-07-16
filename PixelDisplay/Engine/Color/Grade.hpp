// PixelDisplay Pro — Engine/Color/Grade.hpp
//
// Colour-grading operators applied to the scene-linear signal that drives the
// display. All operate in linear light so exposure/contrast behave physically.
// Order is chosen to mirror a conventional grade: exposure → white balance/tint
// → contrast (log-ish pivot) → shadow lift / highlight compression →
// saturation → vibrance → brightness. Highlight compression provides the
// no-clip rolloff; nothing here hard-clamps, so HDR headroom is preserved.
#pragma once

#include <algorithm>
#include <cmath>

#include "Engine/Core/ParamSnapshot.hpp"
#include "Engine/Math/Vec.hpp"

namespace pd::color {

using math::Vec3;

inline float luminance(Vec3 c) {
    return 0.2126390f * c.x + 0.7151687f * c.y + 0.0721923f * c.z;  // Rec.709
}

/// Soft highlight rolloff: values below `knee` pass through; above it they are
/// compressed toward an asymptote so bright emission never hard-clips.
inline float highlightRolloff(float v, float amount) {
    if (amount <= 0.0f || v <= 1.0f) return v;
    // Reinhard-style compression that starts biting near 1.0.
    float k = 1.0f + amount * 4.0f;
    return v <= 1.0f ? v : 1.0f + (v - 1.0f) / (1.0f + (v - 1.0f) * k);
}

inline Vec3 applyGrade(Vec3 c, const ColorParams& p) {
    // Exposure (stops) in linear light.
    if (p.exposure != 0.0f) {
        float g = std::pow(2.0f, p.exposure);
        c = c * g;
    }

    // White balance (warm/cool) and tint (green/magenta), gentle channel gains.
    if (p.whiteBalance != 0.0f || p.tint != 0.0f) {
        float wb = p.whiteBalance;
        float ti = p.tint;
        c.x *= 1.0f + 0.30f * wb;                 // more red when warm
        c.z *= 1.0f - 0.30f * wb;                 // less blue when warm
        c.y *= 1.0f + 0.20f * ti;                 // tint toward green (+) / magenta (-)
        c.x *= 1.0f - 0.10f * ti;
        c.z *= 1.0f - 0.10f * ti;
    }

    // Contrast around a linear mid-grey pivot (0.18).
    if (p.contrast != 1.0f) {
        constexpr float pivot = 0.18f;
        c.x = (c.x - pivot) * p.contrast + pivot;
        c.y = (c.y - pivot) * p.contrast + pivot;
        c.z = (c.z - pivot) * p.contrast + pivot;
    }

    // Shadow lift (raise blacks toward a fraction of mid grey).
    if (p.shadowLift != 0.0f) {
        float lift = p.shadowLift * 0.1f;
        auto liftCh = [&](float v) {
            float t = std::max(0.0f, 1.0f - v * 4.0f);  // strongest in deep shadow
            return v + lift * t;
        };
        c = {liftCh(c.x), liftCh(c.y), liftCh(c.z)};
    }

    // Highlight compression (no-clip rolloff).
    if (p.highlightCompression > 0.0f) {
        c = {highlightRolloff(c.x, p.highlightCompression),
             highlightRolloff(c.y, p.highlightCompression),
             highlightRolloff(c.z, p.highlightCompression)};
    }

    // Saturation (mix toward luminance).
    if (p.saturation != 1.0f) {
        float l = luminance(c);
        c = Vec3{l, l, l} + (c - Vec3{l, l, l}) * p.saturation;
    }

    // Vibrance: boost saturation more where the pixel is currently less saturated.
    if (p.vibrance != 0.0f) {
        float l = luminance(c);
        float mx = std::max({c.x, c.y, c.z});
        float mn = std::min({c.x, c.y, c.z});
        float sat = mx > 1e-5f ? (mx - mn) / mx : 0.0f;
        float boost = 1.0f + p.vibrance * (1.0f - sat);
        c = Vec3{l, l, l} + (c - Vec3{l, l, l}) * boost;
    }

    // Brightness: gentle additive offset in linear light.
    if (p.brightness != 0.0f) {
        float b = p.brightness * 0.1f;
        c = c + Vec3{b, b, b};
    }

    // Gamma (applied as a display-side power on the driving signal).
    if (p.gamma != 1.0f && p.gamma > 0.0f) {
        float ig = 1.0f / p.gamma;
        auto safe = [ig](float v) { return v > 0.0f ? std::pow(v, ig) : v; };
        c = {safe(c.x), safe(c.y), safe(c.z)};
    }

    return c;
}

}  // namespace pd::color
