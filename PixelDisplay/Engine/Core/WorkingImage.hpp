// PixelDisplay Pro — Engine/Core/WorkingImage.hpp
//
// The engine's internal working buffer: planar-interleaved RGBA float, always
// in scene-linear space once decoded. Owned by the RenderContext for the
// duration of a single render call (never shared across threads / frames).
#pragma once

#include <cstdint>
#include <vector>

#include "Engine/Core/PixelAccess.hpp"

namespace pd {

class WorkingImage {
public:
    WorkingImage() = default;
    WorkingImage(std::int32_t w, std::int32_t h) { resize(w, h); }

    void resize(std::int32_t w, std::int32_t h) {
        width_ = w;
        height_ = h;
        pixels_.assign(static_cast<std::size_t>(w) * h, RGBA{});
    }

    std::int32_t width() const { return width_; }
    std::int32_t height() const { return height_; }
    bool empty() const { return pixels_.empty(); }

    RGBA& at(std::int32_t x, std::int32_t y) {
        return pixels_[static_cast<std::size_t>(y) * width_ + x];
    }
    const RGBA& at(std::int32_t x, std::int32_t y) const {
        return pixels_[static_cast<std::size_t>(y) * width_ + x];
    }

    RGBA* row(std::int32_t y) { return pixels_.data() + static_cast<std::size_t>(y) * width_; }
    const RGBA* row(std::int32_t y) const { return pixels_.data() + static_cast<std::size_t>(y) * width_; }

    RGBA* data() { return pixels_.data(); }
    const RGBA* data() const { return pixels_.data(); }

    void swap(WorkingImage& other) noexcept {
        std::swap(width_, other.width_);
        std::swap(height_, other.height_);
        pixels_.swap(other.pixels_);
    }

private:
    std::int32_t width_ = 0, height_ = 0;
    std::vector<RGBA> pixels_;
};

}  // namespace pd
