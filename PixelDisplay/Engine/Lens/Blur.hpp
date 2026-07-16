// PixelDisplay Pro — Engine/Lens/Blur.hpp
//
// Separable Gaussian blur over the linear working image, used by glow, bloom,
// lens blur and camera defocus. Two 1D passes (horizontal then vertical) with
// clamped edges. Kernel is built once per radius and reused across both passes.
//
// Blurs run in scene-linear light so bright emitters bloom correctly.
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Engine/Core/WorkingImage.hpp"

namespace pd::lens {

/// Gaussian blur `src` -> `dst` with the given radius (pixels). `tmp` is scratch
/// of the same size (provided by the caller to avoid per-call allocation).
inline void gaussianBlur(const WorkingImage& src, WorkingImage& dst, WorkingImage& tmp,
                         float radius) {
    const int w = src.width();
    const int h = src.height();
    if (radius <= 0.01f) { dst = src; return; }

    const float sigma = radius * 0.5f + 0.5f;
    const int r = std::max(1, static_cast<int>(std::ceil(radius * 2.0f)));
    std::vector<float> kernel(2 * r + 1);
    float sum = 0.0f;
    const float inv2s2 = 1.0f / (2.0f * sigma * sigma);
    for (int i = -r; i <= r; ++i) {
        float wgt = std::exp(-static_cast<float>(i * i) * inv2s2);
        kernel[i + r] = wgt;
        sum += wgt;
    }
    for (float& k : kernel) k /= sum;

    // Horizontal pass: src -> tmp.
    for (int y = 0; y < h; ++y) {
        const RGBA* srow = src.row(y);
        RGBA* trow = tmp.row(y);
        for (int x = 0; x < w; ++x) {
            RGBA acc{0, 0, 0, 0};
            for (int i = -r; i <= r; ++i) {
                int sx = std::clamp(x + i, 0, w - 1);
                float k = kernel[i + r];
                acc.r += srow[sx].r * k;
                acc.g += srow[sx].g * k;
                acc.b += srow[sx].b * k;
                acc.a += srow[sx].a * k;
            }
            trow[x] = acc;
        }
    }

    // Vertical pass: tmp -> dst.
    for (int y = 0; y < h; ++y) {
        RGBA* drow = dst.row(y);
        for (int x = 0; x < w; ++x) {
            RGBA acc{0, 0, 0, 0};
            for (int i = -r; i <= r; ++i) {
                int sy = std::clamp(y + i, 0, h - 1);
                float k = kernel[i + r];
                const RGBA& p = tmp.row(sy)[x];
                acc.r += p.r * k;
                acc.g += p.g * k;
                acc.b += p.b * k;
                acc.a += p.a * k;
            }
            drow[x] = acc;
        }
    }
}

}  // namespace pd::lens
