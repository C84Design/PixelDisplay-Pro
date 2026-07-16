// PixelDisplay Pro — Engine/Renderer/Stages/OpticsStage.cpp
#include "Engine/Renderer/Stages/OpticsStage.hpp"

#include <algorithm>
#include <cmath>

#include "Engine/Lens/Blur.hpp"
#include "Engine/Math/Vec.hpp"
#include "Engine/Sampling/Resampler.hpp"

namespace pd::stages {

using math::Vec2;
using math::Vec3;
using math::clampf;

void OpticsStage::executeCpu(RenderContext& ctx) const {
    const ParamSnapshot& snap = ctx.params();
    const LensParams& L = snap.lens;
    const DisplayCharacteristics& chr = snap.characteristics;
    const WorkingImage& srcImg = ctx.front();
    WorkingImage& dstImg = ctx.back();

    const int w = srcImg.width();
    const int h = srcImg.height();
    if (w == 0 || h == 0) return;

    const Vec2 center{w * 0.5f, h * 0.5f};

    // --- Geometric resample: screen curvature (barrel) + lens chromatic
    // aberration (per-channel radial scale) + a faint refraction ghost. --------
    const bool geo = L.screenCurvature > 0.0f || L.chromaticAberration > 0.0f ||
                     L.refraction > 0.0f;
    WorkingImage geoImg(w, h);
    if (geo) {
        const float curv = L.screenCurvature;
        const float ca = L.chromaticAberration;
        const float refr = L.refraction * (0.5f + L.glassThickness);
        for (int y = 0; y < h; ++y) {
            RGBA* row = geoImg.row(y);
            for (int x = 0; x < w; ++x) {
                Vec2 uv{(x + 0.5f - center.x) / center.x,
                        (y + 0.5f - center.y) / center.y};   // [-1,1]
                float r2 = uv.x * uv.x + uv.y * uv.y;
                float bulge = 1.0f + curv * 0.30f * r2;      // barrel distortion

                auto sampleCh = [&](float chScale) -> RGBA {
                    Vec2 d = uv * (bulge * chScale);
                    Vec2 pos{center.x + d.x * center.x, center.y + d.y * center.y};
                    return sampling::sampleBilinear(srcImg, pos.x, pos.y);
                };
                RGBA cr = sampleCh(1.0f + ca * 0.01f);
                RGBA cg = sampleCh(1.0f);
                RGBA cb = sampleCh(1.0f - ca * 0.01f);
                RGBA out{cr.r, cg.g, cb.b, cg.a};

                if (refr > 0.0f) {
                    Vec2 pos{x + 0.5f + refr * 3.0f, y + 0.5f + refr * 1.5f};
                    RGBA ghost = sampling::sampleBilinear(srcImg, pos.x, pos.y);
                    float m = clampf(refr, 0.0f, 1.0f) * 0.35f;
                    out = {out.r * (1 - m) + ghost.r * m, out.g * (1 - m) + ghost.g * m,
                           out.b * (1 - m) + ghost.b * m, out.a};
                }
                row[x] = out;
            }
        }
    } else {
        geoImg = srcImg;
    }

    // --- Blur-based effects: lens blur / defocus, then glow and bloom. --------
    WorkingImage tmp(w, h);
    WorkingImage work = geoImg;

    float focusBlur = std::max(L.lensBlur, L.cameraDefocus);
    if (focusBlur > 0.0f) {
        WorkingImage blurred(w, h);
        lens::gaussianBlur(work, blurred, tmp, focusBlur);
        work.swap(blurred);
    }

    // Pixel glow: soft halo around lit emitters (additive).
    WorkingImage glow(w, h);
    bool haveGlow = chr.glowIntensity > 0.0f && chr.glowRadius > 0.0f;
    if (haveGlow) lens::gaussianBlur(work, glow, tmp, chr.glowRadius);

    // Bloom: threshold the bright emitters, blur, add back.
    WorkingImage bloom(w, h);
    bool haveBloom = chr.bloomIntensity > 0.0f;
    if (haveBloom) {
        WorkingImage bright(w, h);
        for (int y = 0; y < h; ++y) {
            const RGBA* wr = work.row(y);
            RGBA* br = bright.row(y);
            for (int x = 0; x < w; ++x) {
                float luma = 0.2126f * wr[x].r + 0.7152f * wr[x].g + 0.0722f * wr[x].b;
                float e = std::max(0.0f, luma - chr.bloomThreshold);
                float s = luma > 1e-5f ? e / luma : 0.0f;
                br[x] = {wr[x].r * s, wr[x].g * s, wr[x].b * s, 0.0f};
            }
        }
        lens::gaussianBlur(bright, bloom, tmp, std::max(chr.glowRadius, 6.0f));
    }

    // --- Composite + per-pixel overlays (reflection, moiré, polarizer, AR). ---
    const float arReduce = 1.0f - clampf(L.antiReflectiveCoating, 0.0f, 1.0f) * 0.8f;
    const float reflAmt = L.reflection * arReduce;
    const float invW = 1.0f / w, invH = 1.0f / h;

    for (int y = 0; y < h; ++y) {
        const RGBA* wr = work.row(y);
        RGBA* out = dstImg.row(y);
        for (int x = 0; x < w; ++x) {
            Vec3 c{wr[x].r, wr[x].g, wr[x].b};

            if (haveGlow) {
                const RGBA& gp = glow.row(y)[x];
                c = c + Vec3{gp.r, gp.g, gp.b} * chr.glowIntensity;
            }
            if (haveBloom) {
                const RGBA& bp = bloom.row(y)[x];
                c = c + Vec3{bp.r, bp.g, bp.b} * chr.bloomIntensity;
            }

            const float nx = (x + 0.5f) * invW - 0.5f;
            const float ny = (y + 0.5f) * invH - 0.5f;

            // Moiré: low-frequency beat between the pixel grid and the sensor.
            if (L.moire > 0.0f) {
                float beat = std::sin((x * 0.37f + y * 0.11f)) *
                             std::sin((x * 0.11f - y * 0.37f));
                c = c * (1.0f + L.moire * 0.15f * beat);
            }

            // Display reflection: soft off-axis highlight across the glass.
            if (reflAmt > 0.0f) {
                float grad = clampf(0.5f + (nx * 0.8f + ny * 0.6f), 0.0f, 1.0f);
                float refl = reflAmt * 0.15f * std::pow(grad, 2.0f);
                c = c + Vec3{refl, refl, refl * 1.1f};
            }

            // Polarizer: mild angle-dependent darkening + cool tint.
            if (L.polarizer > 0.0f) {
                float f = 1.0f - L.polarizer * 0.25f;
                c = c * Vec3{f, f, f * 1.03f};
            }

            // Anti-reflective coating residual: faint green/purple bloom tint.
            if (L.antiReflectiveCoating > 0.0f) {
                float a = L.antiReflectiveCoating * 0.03f;
                c = c + Vec3{a * 0.4f, a, a * 0.8f};
            }

            out[x] = {c.x, c.y, c.z, wr[x].a};
        }
    }
}

}  // namespace pd::stages
