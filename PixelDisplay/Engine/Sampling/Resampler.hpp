// PixelDisplay Pro — Engine/Sampling/Resampler.hpp
//
// Source resampling for the display grid. The display cells sample the source
// signal at their centroids; higher-quality kernels (bicubic/Lanczos) arrive
// with the color pipeline milestone. Milestone 2 provides nearest + bilinear.
//
// Coordinates are in pixel space with pixel centers at integer+0.5 (u,v are in
// pixels). Edge handling clamps to the image border.
#pragma once

#include <algorithm>

#include "Engine/Core/WorkingImage.hpp"

namespace pd::sampling {

inline RGBA sampleNearest(const WorkingImage& img, float u, float v) {
    int x = std::clamp(static_cast<int>(u), 0, img.width() - 1);
    int y = std::clamp(static_cast<int>(v), 0, img.height() - 1);
    return img.at(x, y);
}

inline RGBA sampleBilinear(const WorkingImage& img, float u, float v) {
    // Shift so that texel centers sit at integer coordinates.
    float fx = u - 0.5f;
    float fy = v - 0.5f;
    int x0 = static_cast<int>(std::floor(fx));
    int y0 = static_cast<int>(std::floor(fy));
    float tx = fx - x0;
    float ty = fy - y0;

    auto clampX = [&](int x) { return std::clamp(x, 0, img.width() - 1); };
    auto clampY = [&](int y) { return std::clamp(y, 0, img.height() - 1); };

    const RGBA& c00 = img.at(clampX(x0),     clampY(y0));
    const RGBA& c10 = img.at(clampX(x0 + 1), clampY(y0));
    const RGBA& c01 = img.at(clampX(x0),     clampY(y0 + 1));
    const RGBA& c11 = img.at(clampX(x0 + 1), clampY(y0 + 1));

    auto mix = [](float a, float b, float t) { return a + (b - a) * t; };
    RGBA top{mix(c00.r, c10.r, tx), mix(c00.g, c10.g, tx),
             mix(c00.b, c10.b, tx), mix(c00.a, c10.a, tx)};
    RGBA bot{mix(c01.r, c11.r, tx), mix(c01.g, c11.g, tx),
             mix(c01.b, c11.b, tx), mix(c01.a, c11.a, tx)};
    return {mix(top.r, bot.r, ty), mix(top.g, bot.g, ty),
            mix(top.b, bot.b, ty), mix(top.a, bot.a, ty)};
}

}  // namespace pd::sampling
