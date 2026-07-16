// PixelDisplay Pro — Engine/Color/Transfer.hpp
//
// Transfer functions (EOTF/OETF) between display-encoded values and
// scene-linear. Internal rendering math is always scene-linear so that light
// (emission, glow, bloom) adds correctly. Milestone 2 uses sRGB / linear; the
// full primaries + P3/Rec.709/Rec.2020 matrices arrive in Milestone 4.
//
// These scalar helpers are the single source of truth: the CPU kernels call
// them directly and the GPU shaders mirror them verbatim (DESIGN.md §7.4).
#pragma once

#include <cmath>

#include "Engine/Core/PixelAccess.hpp"
#include "Engine/Core/Types.hpp"

namespace pd::color {

/// sRGB electro-optical transfer (encoded [0,1] -> linear). Values outside
/// [0,1] are extrapolated linearly so HDR highlights survive round-trips.
inline float srgbToLinear(float c) {
    if (c < 0.0f) return c;  // preserve sign / HDR headroom
    if (c <= 0.04045f) return c / 12.92f;
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}

/// Inverse sRGB (linear -> encoded [0,1]).
inline float linearToSrgb(float c) {
    if (c < 0.0f) return c;
    if (c <= 0.0031308f) return c * 12.92f;
    return 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
}

/// Rec.709 uses effectively the sRGB primaries; for M2 the transfer is treated
/// as sRGB-equivalent. (BT.1886 nuance handled in M4.)
inline float decode(float c, ColorSpace space) {
    switch (space) {
        case ColorSpace::Linear:  return c;
        case ColorSpace::sRGB:
        case ColorSpace::DisplayP3:
        case ColorSpace::Rec709:
        case ColorSpace::Rec2020: return srgbToLinear(c);
    }
    return c;
}

inline float encode(float c, ColorSpace space) {
    switch (space) {
        case ColorSpace::Linear:  return c;
        case ColorSpace::sRGB:
        case ColorSpace::DisplayP3:
        case ColorSpace::Rec709:
        case ColorSpace::Rec2020: return linearToSrgb(c);
    }
    return c;
}

/// Decode an RGBA sample's colour channels to linear (alpha untouched).
inline RGBA decodeRGBA(RGBA c, ColorSpace space) {
    return {decode(c.r, space), decode(c.g, space), decode(c.b, space), c.a};
}
inline RGBA encodeRGBA(RGBA c, ColorSpace space) {
    return {encode(c.r, space), encode(c.g, space), encode(c.b, space), c.a};
}

}  // namespace pd::color
