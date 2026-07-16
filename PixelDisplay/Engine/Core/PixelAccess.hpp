// PixelDisplay Pro — Engine/Core/PixelAccess.hpp
//
// Format-aware load/store of a single pixel as a normalized RGBA Vec4. Integer
// formats are normalized to [0,1]; float formats pass through unclamped so HDR
// highlights survive the pipeline. Channel order (ARGB vs RGBA) is resolved
// here so the rest of the engine always works in RGBA order.
//
// NOTE on 16-bit: the engine treats *16 formats as full-range 0..65535. After
// Effects' 16-bit world is 0..0x8000; the AE host adapter is responsible for
// presenting/scaling that quirk when it builds the ImageView.
#pragma once

#include <cstdint>

#include "Engine/Core/Types.hpp"
#include "Engine/Math/Vec.hpp"

namespace pd {

struct RGBA {
    float r = 0, g = 0, b = 0, a = 1;
};

namespace detail {
inline RGBA fromU8(const std::uint8_t* p, bool argb) {
    constexpr float inv = 1.0f / 255.0f;
    if (argb) return {p[1] * inv, p[2] * inv, p[3] * inv, p[0] * inv};
    return {p[0] * inv, p[1] * inv, p[2] * inv, p[3] * inv};
}
inline RGBA fromU16(const std::uint16_t* p, bool argb) {
    constexpr float inv = 1.0f / 65535.0f;
    if (argb) return {p[1] * inv, p[2] * inv, p[3] * inv, p[0] * inv};
    return {p[0] * inv, p[1] * inv, p[2] * inv, p[3] * inv};
}
inline RGBA fromF32(const float* p, bool argb) {
    if (argb) return {p[1], p[2], p[3], p[0]};
    return {p[0], p[1], p[2], p[3]};
}
inline std::uint8_t toU8(float v) {
    float s = v < 0 ? 0 : (v > 1 ? 1 : v);
    return static_cast<std::uint8_t>(s * 255.0f + 0.5f);
}
inline std::uint16_t toU16(float v) {
    float s = v < 0 ? 0 : (v > 1 ? 1 : v);
    return static_cast<std::uint16_t>(s * 65535.0f + 0.5f);
}
inline void storeU8(std::uint8_t* p, RGBA c, bool argb) {
    if (argb) { p[0] = toU8(c.a); p[1] = toU8(c.r); p[2] = toU8(c.g); p[3] = toU8(c.b); }
    else      { p[0] = toU8(c.r); p[1] = toU8(c.g); p[2] = toU8(c.b); p[3] = toU8(c.a); }
}
inline void storeU16(std::uint16_t* p, RGBA c, bool argb) {
    if (argb) { p[0] = toU16(c.a); p[1] = toU16(c.r); p[2] = toU16(c.g); p[3] = toU16(c.b); }
    else      { p[0] = toU16(c.r); p[1] = toU16(c.g); p[2] = toU16(c.b); p[3] = toU16(c.a); }
}
inline void storeF32(float* p, RGBA c, bool argb) {
    if (argb) { p[0] = c.a; p[1] = c.r; p[2] = c.g; p[3] = c.b; }
    else      { p[0] = c.r; p[1] = c.g; p[2] = c.b; p[3] = c.a; }
}
}  // namespace detail

inline RGBA loadPixel(const ImageView& img, std::int32_t x, std::int32_t y) {
    const std::uint8_t* row = img.rowPtr(y);
    switch (img.format) {
        case PixelFormat::ARGB8:   return detail::fromU8(row + x * 4, true);
        case PixelFormat::RGBA8:   return detail::fromU8(row + x * 4, false);
        case PixelFormat::ARGB16:  return detail::fromU16(reinterpret_cast<const std::uint16_t*>(row) + x * 4, true);
        case PixelFormat::RGBA16:  return detail::fromU16(reinterpret_cast<const std::uint16_t*>(row) + x * 4, false);
        case PixelFormat::ARGB32F: return detail::fromF32(reinterpret_cast<const float*>(row) + x * 4, true);
        case PixelFormat::RGBA32F: return detail::fromF32(reinterpret_cast<const float*>(row) + x * 4, false);
    }
    return {};
}

inline void storePixel(const ImageView& img, std::int32_t x, std::int32_t y, RGBA c) {
    std::uint8_t* row = img.rowPtr(y);
    switch (img.format) {
        case PixelFormat::ARGB8:   detail::storeU8(row + x * 4, c, true); break;
        case PixelFormat::RGBA8:   detail::storeU8(row + x * 4, c, false); break;
        case PixelFormat::ARGB16:  detail::storeU16(reinterpret_cast<std::uint16_t*>(row) + x * 4, c, true); break;
        case PixelFormat::RGBA16:  detail::storeU16(reinterpret_cast<std::uint16_t*>(row) + x * 4, c, false); break;
        case PixelFormat::ARGB32F: detail::storeF32(reinterpret_cast<float*>(row) + x * 4, c, true); break;
        case PixelFormat::RGBA32F: detail::storeF32(reinterpret_cast<float*>(row) + x * 4, c, false); break;
    }
}

}  // namespace pd
