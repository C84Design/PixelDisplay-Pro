// PixelDisplay Pro — Engine/Math/Sdf.hpp
//
// Signed-distance functions for procedural emitter footprints. Layouts are
// described analytically (never as bitmaps), and coverage is evaluated per
// output pixel from these SDFs with analytic anti-aliasing. Distances are in
// the local cell coordinate system where the cell spans roughly [-0.5, 0.5].
//
// Convention: sdf < 0 inside the shape, > 0 outside. `coverage()` converts a
// distance to [0,1] using the pixel's footprint so edges are smooth and
// resolution-independent.
#pragma once

#include <algorithm>
#include <cmath>

#include "Engine/Math/Vec.hpp"

namespace pd::math {

/// Signed distance to an axis-aligned box of half-extents b, centered at origin.
inline float sdfBox(Vec2 p, Vec2 b) {
    float dx = std::fabs(p.x) - b.x;
    float dy = std::fabs(p.y) - b.y;
    float outside = std::sqrt(std::max(dx, 0.0f) * std::max(dx, 0.0f) +
                              std::max(dy, 0.0f) * std::max(dy, 0.0f));
    float inside = std::min(std::max(dx, dy), 0.0f);
    return outside + inside;
}

/// Signed distance to a rounded box (half-extents b, corner radius r).
inline float sdfRoundedBox(Vec2 p, Vec2 b, float r) {
    r = std::min(r, std::min(b.x, b.y));
    Vec2 bb{b.x - r, b.y - r};
    return sdfBox(p, bb) - r;
}

/// Signed distance to a circle of radius r centered at origin.
inline float sdfCircle(Vec2 p, float r) {
    return std::sqrt(p.x * p.x + p.y * p.y) - r;
}

/// Signed distance to a regular (pointy-top) hexagon of inradius r.
/// Canonical construction: fold into one sextant, then distance to the edge.
inline float sdfHexagon(Vec2 p, float r) {
    constexpr float kx = -0.866025404f;  // -cos(30°)
    constexpr float ky = 0.5f;           //  sin(30°)
    constexpr float kz = 0.577350269f;   //  tan(30°)
    p.x = std::fabs(p.x);
    p.y = std::fabs(p.y);
    float dot2 = 2.0f * std::min(kx * p.x + ky * p.y, 0.0f);
    p.x -= dot2 * kx;
    p.y -= dot2 * ky;
    p.x -= clampf(p.x, -kz * r, kz * r);
    p.y -= r;
    float len = std::sqrt(p.x * p.x + p.y * p.y);
    return len * (p.y < 0.0f ? -1.0f : 1.0f);
}

/// Convert a signed distance to coverage in [0,1]. `aa` is the half-width of the
/// transition band (in the same units as the distance) — larger = softer edge.
inline float coverage(float sdf, float aa) {
    if (aa <= 0.0f) return sdf <= 0.0f ? 1.0f : 0.0f;
    // Inside (sdf<0) -> 1, outside -> 0, smooth across [-aa, +aa].
    return 1.0f - smoothstepf(-aa, aa, sdf);
}

}  // namespace pd::math
