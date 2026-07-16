// PixelDisplay Pro — Engine/Core/Types.hpp
//
// Public, host-facing value types that cross the engine boundary. These are the
// ONLY types a host adapter needs to construct in order to drive a render.
// They are plain data (no Adobe headers, no engine internals).
#pragma once

#include <cstddef>
#include <cstdint>

#include "Engine/Math/Vec.hpp"

namespace pd {

/// Pixel storage layout of a host buffer. Channel order + bit depth.
/// After Effects delivers ARGB; most GPU APIs prefer RGBA. The engine reads and
/// writes whatever the host hands it via ImageView, so both orders are modeled.
enum class PixelFormat {
    ARGB8,    // 8-bit unsigned, A,R,G,B byte order (After Effects 8-bit world)
    ARGB16,   // 16-bit unsigned (AE 16-bit world is 0..0x8000 half-float-ish; see note)
    ARGB32F,  // 32-bit float per channel (AE 32-bit float world)
    RGBA8,
    RGBA16,
    RGBA32F,
};

/// Working/encoding color spaces. Internal math is always scene-linear.
enum class ColorSpace {
    Linear,       // scene-linear (engine working space)
    sRGB,         // sRGB primaries + sRGB transfer
    DisplayP3,    // P3 primaries + sRGB transfer
    Rec709,       // Rec.709 primaries + Rec.709 (BT.1886-ish) transfer
    Rec2020,      // Rec.2020 primaries
};

inline std::size_t bytesPerPixel(PixelFormat f) {
    switch (f) {
        case PixelFormat::ARGB8:
        case PixelFormat::RGBA8:   return 4;
        case PixelFormat::ARGB16:
        case PixelFormat::RGBA16:  return 8;
        case PixelFormat::ARGB32F:
        case PixelFormat::RGBA32F: return 16;
    }
    return 0;
}

/// Non-owning descriptor of a pixel buffer. Ownership stays with the creator
/// (the host). The engine never frees, resizes, or retains this beyond a call.
struct ImageView {
    void* data = nullptr;         // base pointer to top-left pixel
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::ptrdiff_t rowBytes = 0;  // stride in bytes (may be negative for flipped buffers)
    PixelFormat format = PixelFormat::ARGB8;
    ColorSpace colorSpace = ColorSpace::sRGB;

    bool valid() const {
        return data != nullptr && width > 0 && height > 0 && rowBytes != 0;
    }

    // Byte address of the start of row y (no bounds check).
    std::uint8_t* rowPtr(std::int32_t y) const {
        return static_cast<std::uint8_t*>(data) + static_cast<std::ptrdiff_t>(y) * rowBytes;
    }
};

/// Temporal information for one frame. Temporal effects are closed-form in
/// `layerTimeSeconds` so that Multi-Frame Rendering stays frame-independent.
struct TimeInfo {
    double layerTimeSeconds = 0.0;  // time within the layer
    double frameRate = 30.0;        // comp frame rate
    std::int64_t frameIndex = 0;    // absolute frame index (for deterministic seeding)
};

/// Quality selector; adaptive/draft modes trade taps & internal resolution.
enum class RenderQuality {
    Draft,
    Preview,
    Final,
};

/// A rectangular sub-region to render (After Effects smart-render output rect).
/// Defaults to the whole image when width/height are zero.
struct RenderRegion {
    std::int32_t x = 0, y = 0, width = 0, height = 0;
    bool full() const { return width == 0 && height == 0; }
};

}  // namespace pd
