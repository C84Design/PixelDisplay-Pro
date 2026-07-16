// PixelDisplay Pro — Engine/DisplayLayouts/LayoutModel.hpp
//
// Analytic emitter-footprint evaluation. Given a point in a cell's local space
// (origin at the cell center, cell spanning ~[-0.5, 0.5]), this returns the
// emitter coverage in [0,1]. Everything is derived from SDFs so the result is
// resolution-independent with analytic anti-aliasing — never a bitmap.
//
// Milestone 2 implements the single-emitter ("whole pixel") shapes:
//   Square, RoundedSquare, Circle, Diamond, Hexagon, and the panel types that
//   visually reduce to a rounded emitter at this stage. RGB subpixel structure
//   (stripe / PenTile / CRT masks) is layered on in Milestone 3.
#pragma once

#include "Engine/Core/ParamSnapshot.hpp"
#include "Engine/Math/Sdf.hpp"
#include "Engine/Math/Vec.hpp"

namespace pd::layout {

using math::Vec2;

/// Emitter half-extents (in cell units) from dot size and spacing.
inline Vec2 emitterHalfExtent(const DisplayParams& d) {
    float base = 0.5f * (1.0f - math::clampf(d.spacing, 0.0f, 0.95f));
    float dot = math::clampf(d.dotSize, 0.0f, 1.5f);
    float hx = base * dot;
    float hy = base * dot;
    // Pixel aspect stretches the emitter horizontally (aspect > 1 => wider).
    if (d.pixelAspect >= 1.0f) hy /= d.pixelAspect;
    else hx *= d.pixelAspect;
    return {hx, hy};
}

/// Coverage of the emitter at local point p (cell-local, centered), with an
/// anti-aliasing half-band `aa` (in cell units).
inline float emitterCoverage(DisplayType type, Vec2 p, const DisplayParams& d, float aa) {
    const Vec2 h = emitterHalfExtent(d);
    const float roundness = math::clampf(d.pixelRoundness, 0.0f, 1.0f);

    float sdf;
    switch (type) {
        case DisplayType::Circular:
            sdf = math::sdfCircle(p, math::clampf(std::min(h.x, h.y), 0.0f, 0.5f));
            break;
        case DisplayType::Diamond:
        case DisplayType::DiamondOled: {
            // 45°-rotated square.
            Vec2 r = math::rotate(p, 0.785398163f);
            sdf = math::sdfRoundedBox(r, h, roundness * std::min(h.x, h.y));
            break;
        }
        case DisplayType::Hexagonal:
            sdf = math::sdfHexagon(p, std::min(h.x, h.y));
            break;
        case DisplayType::Square:
            sdf = math::sdfBox(p, h);
            break;
        case DisplayType::RoundedSquare:
        default:
            // Default emitter for panels whose subpixel structure is added in M3.
            sdf = math::sdfRoundedBox(p, h, roundness * std::min(h.x, h.y));
            break;
    }
    return math::coverage(sdf, aa);
}

}  // namespace pd::layout
