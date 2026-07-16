// PixelDisplay Pro — Engine/Renderer/Stages/SynthesisStage.cpp
#include "Engine/Renderer/Stages/SynthesisStage.hpp"

#include <cmath>

#include "Engine/Color/Grade.hpp"
#include "Engine/Color/Primaries.hpp"
#include "Engine/Color/Transfer.hpp"
#include "Engine/DisplayLayouts/LayoutModel.hpp"
#include "Engine/Math/Vec.hpp"
#include "Engine/Noise/Hash.hpp"
#include "Engine/Sampling/Resampler.hpp"

namespace pd::stages {

using math::Vec2;
using math::Vec3;

namespace {

/// The signal a subpixel of `channel` emits, after subpixel gamma & RGB scale.
/// Accumulates into `emit` (the emitted linear RGB) weighted by coverage.
inline void accumulate(Vec3& emit, int channel, const RGBA& lin, float cov,
                       const SubpixelParams& sp) {
    auto shape = [&](float v) { return std::pow(std::max(v, 0.0f), sp.gamma); };
    switch (channel) {
        case 0: emit.x += shape(lin.r) * sp.scaleR * cov; break;
        case 1: emit.y += shape(lin.g) * sp.scaleG * cov; break;
        case 2: emit.z += shape(lin.b) * sp.scaleB * cov; break;
        default:  // white / full-colour element
            emit.x += shape(lin.r) * sp.scaleR * cov;
            emit.y += shape(lin.g) * sp.scaleG * cov;
            emit.z += shape(lin.b) * sp.scaleB * cov;
            break;
    }
}

}  // namespace

void SynthesisStage::executeCpu(RenderContext& ctx) const {
    const ParamSnapshot& snap = ctx.params();
    const DisplayParams& d = snap.display;
    const ColorParams& col = snap.color;
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

    const float aa = 0.5f / pitch + d.edgeSoftening + d.softness;
    const float randomness = math::clampf(d.pixelRandomness, 0.0f, 1.0f);
    const float brightComp = std::max(d.brightnessCompensation, 0.0f);

    // Colour setup. When linear workflow is off we treat the signal as already
    // linear-coded (no transfer, no gamut change) — a deliberate "flat" look.
    const ColorSpace inSpace = col.linearWorkflow ? col.inputSpace : ColorSpace::Linear;
    const math::Mat3 inMat = color::inputToWorking(inSpace);
    const bool caActive = col.caEnable || col.caAmount != 0.0f ||
                          col.offsetR.x != 0 || col.offsetR.y != 0 ||
                          col.offsetG.x != 0 || col.offsetG.y != 0 ||
                          col.offsetB.x != 0 || col.offsetB.y != 0;

    // Per-channel chromatic-aberration / independent RGB sample offsets.
    auto channelOffset = [&](int ch, Vec2 pos) -> Vec2 {
        Vec2 o = ch == 0 ? col.offsetR : (ch == 1 ? col.offsetG : col.offsetB);
        if (col.caEnable && col.caAmount != 0.0f) {
            const float chScale = ch == 0 ? 1.0f : (ch == 2 ? -1.0f : 0.0f);
            Vec2 dir{0, 0};
            switch (col.caDirection) {
                case CaDirection::Horizontal: dir = {1, 0}; break;
                case CaDirection::Vertical:   dir = {0, 1}; break;
                case CaDirection::Radial: {
                    Vec2 r = pos - center;
                    float len = std::sqrt(r.x * r.x + r.y * r.y);
                    float maxr = std::sqrt(center.x * center.x + center.y * center.y);
                    if (len > 1e-4f) dir = r * ((len / std::max(maxr, 1.0f)) / len);
                    break;
                }
            }
            o = o + dir * (col.caAmount * chScale);
        }
        return o;
    };

    // Sample the driving signal at `pos`, decode to linear, convert to working
    // primaries, and grade. Chromatic aberration samples each channel separately.
    auto sampleWorking = [&](Vec2 pos) -> RGBA {
        float r, g, b, a;
        if (caActive) {
            RGBA sr = sampling::sampleBilinear(src, pos.x + channelOffset(0, pos).x,
                                               pos.y + channelOffset(0, pos).y);
            RGBA sg = sampling::sampleBilinear(src, pos.x + channelOffset(1, pos).x,
                                               pos.y + channelOffset(1, pos).y);
            RGBA sb = sampling::sampleBilinear(src, pos.x + channelOffset(2, pos).x,
                                               pos.y + channelOffset(2, pos).y);
            r = color::decode(sr.r, inSpace);
            g = color::decode(sg.g, inSpace);
            b = color::decode(sb.b, inSpace);
            a = sg.a;
        } else {
            RGBA s = sampling::sampleBilinear(src, pos.x, pos.y);
            r = color::decode(s.r, inSpace);
            g = color::decode(s.g, inSpace);
            b = color::decode(s.b, inSpace);
            a = s.a;
        }
        Vec3 work = inMat * Vec3{r, g, b};
        work = color::applyGrade(work, col);
        return {work.x, work.y, work.z, a};
    };

    for (int y = 0; y < h; ++y) {
        RGBA* out = dst.row(y);
        for (int x = 0; x < w; ++x) {
            Vec2 p{x + 0.5f, y + 0.5f};
            Vec2 g = math::rotate(p - center, gridRot) + center + gridOffset;

            int cx = static_cast<int>(std::floor(g.x / pitch));
            int cy = static_cast<int>(std::floor(g.y / pitch));

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
            Vec2 sampleGrid = cellCenter - gridOffset;
            Vec2 samplePos = math::rotate(sampleGrid - center, -gridRot) + center;
            RGBA lin = sampleWorking(samplePos);  // working-linear, graded

            Vec2 local{g.x / pitch - (cx + 0.5f), g.y / pitch - (cy + 0.5f)};
            local = math::rotate(local, pixRot);

            Vec3 emit{0, 0, 0};
            if (snap.subpixel.enable && layout::hasSubpixelStructure(snap.displayType)) {
                layout::Subpixel sub[layout::kMaxSubpixels];
                int n = layout::buildSubpixels(snap.displayType, d, snap.subpixel,
                                               cx & 1, cy & 1, sub);
                for (int i = 0; i < n; ++i) {
                    float cov = layout::subpixelCoverage(sub[i], local, aa);
                    if (cov > 0.0f) accumulate(emit, sub[i].channel, lin, cov, snap.subpixel);
                }
                emit = emit * snap.subpixel.brightness;
            } else {
                float cov = layout::emitterCoverage(snap.displayType, local, d, aa);
                emit = Vec3{lin.r, lin.g, lin.b} * cov;
            }

            float k = brightComp * brightJitter;
            // Emit in SCENE-LINEAR working space. Downstream stages (artifacts,
            // temporal, optics) operate in linear; the ColorEncode stage converts
            // to the output gamut/transfer at the very end (DESIGN.md §4 step 12).
            out[x] = {emit.x * k, emit.y * k, emit.z * k, lin.a};
        }
    }
}

}  // namespace pd::stages
