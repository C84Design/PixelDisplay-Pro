// PixelDisplay Pro — Engine/Renderer/Stages/SynthesisStage.cpp
#include "Engine/Renderer/Stages/SynthesisStage.hpp"

#include <cmath>

#include "Engine/Color/Transfer.hpp"
#include "Engine/DisplayLayouts/LayoutModel.hpp"
#include "Engine/Math/Vec.hpp"
#include "Engine/Noise/Hash.hpp"
#include "Engine/Sampling/Resampler.hpp"

namespace pd::stages {

using math::Vec2;

void SynthesisStage::executeCpu(RenderContext& ctx) const {
    const ParamSnapshot& snap = ctx.params();
    const DisplayParams& d = snap.display;
    const WorkingImage& src = ctx.front();
    WorkingImage& dst = ctx.back();

    const int w = src.width();
    const int h = src.height();
    if (w == 0 || h == 0) return;

    // Grid geometry. pixelSize is the cell pitch in source pixels; guard so the
    // grid never degenerates below one pixel.
    const float scale = std::max(d.resolutionScale, 0.01f);
    const float pitch = std::max(d.pixelSize * scale, 1.0f);
    const Vec2 center{w * 0.5f, h * 0.5f};
    const float gridRot = -d.gridRotationDeg * 3.14159265358979f / 180.0f;
    const float pixRot = -d.pixelRotationDeg * 3.14159265358979f / 180.0f;
    const Vec2 gridOffset{d.gridOffsetX, d.gridOffsetY};

    // Anti-aliasing band: one output pixel spans 1/pitch of a cell; add the
    // artistic softness controls on top.
    const float aa = 0.5f / pitch + d.edgeSoftening + d.softness;
    const float randomness = math::clampf(d.pixelRandomness, 0.0f, 1.0f);
    const float brightComp = std::max(d.brightnessCompensation, 0.0f);

    for (int y = 0; y < h; ++y) {
        RGBA* out = dst.row(y);
        for (int x = 0; x < w; ++x) {
            // Pixel-center coordinate, transformed into grid space.
            Vec2 p{x + 0.5f, y + 0.5f};
            Vec2 g = math::rotate(p - center, gridRot) + center + gridOffset;

            // Cell index and centroid.
            int cx = static_cast<int>(std::floor(g.x / pitch));
            int cy = static_cast<int>(std::floor(g.y / pitch));

            // Optional per-cell positional jitter (seeded, deterministic).
            Vec2 jitter{0, 0};
            float brightJitter = 1.0f;
            if (randomness > 0.0f) {
                float jx, jy;
                noise::hash2f2(cx, cy, d.randomSeed, jx, jy);
                jitter = Vec2{(jx - 0.5f), (jy - 0.5f)} * (randomness * 0.5f);
                float jb = noise::hash2f(cx, cy, d.randomSeed ^ 0x1234u);
                brightJitter = 1.0f - randomness * 0.5f * jb;
            }

            Vec2 cellCenter{(cx + 0.5f + jitter.x) * pitch, (cy + 0.5f + jitter.y) * pitch};

            // Resample the source signal at the cell centroid, then linearize.
            // The centroid is mapped back out of grid space to sample the source.
            Vec2 sampleGrid = cellCenter - gridOffset;
            Vec2 samplePos = math::rotate(sampleGrid - center, -gridRot) + center;
            RGBA sig = sampling::sampleBilinear(src, samplePos.x, samplePos.y);
            RGBA lin = color::decodeRGBA(sig, snap.color.inputSpace);

            // Local coordinate within the cell, in [-0.5, 0.5], with pixel rot.
            Vec2 local{g.x / pitch - (cx + 0.5f), g.y / pitch - (cy + 0.5f)};
            local = math::rotate(local, pixRot);

            float cov = layout::emitterCoverage(snap.displayType, local, d, aa);
            float emit = cov * brightComp * brightJitter;

            RGBA emitted{lin.r * emit, lin.g * emit, lin.b * emit, sig.a};
            out[x] = color::encodeRGBA(emitted, snap.color.outputSpace);
            out[x].a = sig.a;  // preserve source alpha (coverage affects colour only)
        }
    }
}

}  // namespace pd::stages
