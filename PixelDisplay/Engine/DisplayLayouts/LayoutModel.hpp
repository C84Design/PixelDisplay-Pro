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

// ---- Subpixel decomposition (Milestone 3) ----------------------------------
//
// A cell can be decomposed into channel-specific subpixels. Each subpixel emits
// exactly one input channel (or all, for a "white" element) through its own
// analytic footprint. This is what distinguishes real display simulation from a
// coloured-dot overlay: an RGB-stripe panel shows three separated primaries.

enum class SubShape { RoundedBox, Circle, Diamond };

/// One emissive sub-element within a display cell (coordinates cell-local).
struct Subpixel {
    int channel = 3;        // 0=R, 1=G, 2=B, 3=White (all channels)
    Vec2 center{0, 0};      // center in [-0.5, 0.5] cell space
    Vec2 half{0.5f, 0.5f};  // half extents
    SubShape shape = SubShape::RoundedBox;
    float corner = 0.0f;    // corner-radius fraction (RoundedBox only)
};

constexpr int kMaxSubpixels = 8;

/// True for display types that have an RGB (or PenTile) subpixel structure, as
/// opposed to the single-emitter geometric shapes (Square/Circle/Hex/...).
inline bool hasSubpixelStructure(DisplayType t) {
    switch (t) {
        case DisplayType::Square:
        case DisplayType::RoundedSquare:
        case DisplayType::Circular:
        case DisplayType::Diamond:
        case DisplayType::Hexagonal:
        case DisplayType::GameBoyLcd:
            return false;
        default:
            return true;
    }
}

namespace detail {

// Fill three vertical stripes (or horizontal-continuous for aperture grille).
inline int stripes(const DisplayParams& d, const SubpixelParams& sp, bool apertureGrille,
                   const int order[3], Subpixel* out) {
    const float gap = math::clampf(sp.gap, 0.0f, 0.9f);
    const float size = math::clampf(sp.size, 0.1f, 1.5f);
    const float hy = apertureGrille ? 0.5f : 0.5f * (1.0f - math::clampf(d.spacing, 0.0f, 0.9f));
    const float stripeHalfX = (1.0f / 6.0f) * (1.0f - gap) * size;
    for (int i = 0; i < 3; ++i) {
        out[i].channel = order[i];
        out[i].center = {(i - 1) * (1.0f / 3.0f), 0.0f};
        out[i].half = {stripeHalfX, hy * (apertureGrille ? 1.0f : (1.0f - gap)) * size};
        out[i].shape = SubShape::RoundedBox;
        out[i].corner = math::clampf(d.pixelRoundness, 0.0f, 1.0f) * 0.4f;
    }
    return 3;
}

// Three circular phosphor/LED dots in a row (CRT shadow mask / LED matrix).
inline int triad(const SubpixelParams& sp, const int order[3], Subpixel* out) {
    const float gap = math::clampf(sp.gap, 0.0f, 0.9f);
    const float size = math::clampf(sp.size, 0.1f, 1.5f);
    const float r = (1.0f / 6.0f) * (1.0f - gap) * size;
    for (int i = 0; i < 3; ++i) {
        out[i].channel = order[i];
        out[i].center = {(i - 1) * (1.0f / 3.0f), 0.0f};
        out[i].half = {r, r};
        out[i].shape = SubShape::Circle;
    }
    return 3;
}

}  // namespace detail

/// Build the subpixels for one cell. `px`,`py` are the cell-index parities,
/// used by patterns that alternate between cells (PenTile, hex-offset CRT).
/// Returns the count written to `out` (0 => use emitterCoverage instead).
inline int buildSubpixels(DisplayType type, const DisplayParams& d, const SubpixelParams& sp,
                          int px, int py, Subpixel* out) {
    // Channel ordering (RGB vs BGR). Explicit BGR wins; else derive from type.
    const bool bgr = (sp.order == SubpixelOrder::BGR) || (type == DisplayType::LcdBgrStripe);
    const int rgb[3] = {0, 1, 2};
    const int bgrOrder[3] = {2, 1, 0};
    const int* order = bgr ? bgrOrder : rgb;

    const float gap = math::clampf(sp.gap, 0.0f, 0.9f);
    const float size = math::clampf(sp.size, 0.1f, 1.5f);

    switch (type) {
        case DisplayType::LcdRgbStripe:
        case DisplayType::LcdBgrStripe:
        case DisplayType::Oled:
        case DisplayType::MiniLed:
        case DisplayType::MicroLed:
        case DisplayType::RetinaLcd:
        case DisplayType::StudioDisplay:
        case DisplayType::MacBookMiniLed:
        case DisplayType::NintendoDs:
            return detail::stripes(d, sp, /*apertureGrille=*/false, order, out);

        case DisplayType::CrtApertureGrille:
            return detail::stripes(d, sp, /*apertureGrille=*/true, order, out);

        case DisplayType::LedBillboard:
        case DisplayType::RgbLedMatrix:
            return detail::triad(sp, order, out);

        case DisplayType::CrtShadowMask: {
            // Triangular RGB phosphor triad with a half-cell horizontal offset on
            // alternate rows (hex packing), the classic dot-mask look.
            const float r = (1.0f / 6.0f) * (1.0f - gap) * size;
            const float dx = (py & 1) ? 0.16f : -0.16f;
            out[0] = {order[0], {dx - 0.20f, -0.12f}, {r, r}, SubShape::Circle, 0};
            out[1] = {order[1], {dx + 0.20f, -0.12f}, {r, r}, SubShape::Circle, 0};
            out[2] = {order[2], {dx + 0.00f, 0.22f},  {r, r}, SubShape::Circle, 0};
            return 3;
        }

        case DisplayType::PentileOled: {
            // RGBG: a full-size green in every cell, with red/blue alternating
            // between cells (the classic PenTile sharing pattern).
            const float rBig = (1.0f / 4.0f) * (1.0f - gap) * size;
            const float rG = rBig * 0.75f;
            const int rbChannel = ((px ^ py) & 1) ? 2 : 0;  // alternate R / B
            out[0] = {rbChannel, {-0.22f, 0.0f}, {rBig, rBig}, SubShape::Circle, 0};
            out[1] = {1,         { 0.22f, 0.0f}, {rG, rG},     SubShape::Circle, 0};
            return 2;
        }

        case DisplayType::DiamondOled:
        case DisplayType::SamsungAmoled: {
            // Diamond PenTile: small green diamonds on a grid, with larger red and
            // blue diamonds alternating between cells.
            const float gHalf = (1.0f / 6.0f) * (1.0f - gap) * size;
            const float rbHalf = (1.0f / 4.5f) * (1.0f - gap) * size;
            const int rbChannel = ((px ^ py) & 1) ? 2 : 0;
            out[0] = {1,         {0.0f, 0.0f},   {gHalf, gHalf},   SubShape::Diamond, 0};
            out[1] = {rbChannel, {0.0f, -0.26f}, {rbHalf, rbHalf}, SubShape::Diamond, 0};
            out[2] = {rbChannel == 0 ? 2 : 0, {0.0f, 0.26f}, {rbHalf, rbHalf}, SubShape::Diamond, 0};
            return 3;
        }

        default:
            return 0;  // whole-pixel shape types handled by emitterCoverage
    }
}

/// Coverage of a single subpixel at cell-local point p.
inline float subpixelCoverage(const Subpixel& s, Vec2 p, float aa) {
    Vec2 lp = p - s.center;
    float sdf;
    switch (s.shape) {
        case SubShape::Circle:
            sdf = math::sdfCircle(lp, std::min(s.half.x, s.half.y));
            break;
        case SubShape::Diamond:
            sdf = math::sdfRoundedBox(math::rotate(lp, 0.785398163f), s.half, 0.0f);
            break;
        case SubShape::RoundedBox:
        default:
            sdf = math::sdfRoundedBox(lp, s.half, s.corner * std::min(s.half.x, s.half.y));
            break;
    }
    return math::coverage(sdf, aa);
}

}  // namespace pd::layout
