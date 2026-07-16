// PixelDisplay Pro — Engine/Renderer/Stages/ImperfectionsStage.cpp
#include "Engine/Renderer/Stages/ImperfectionsStage.hpp"

#include <algorithm>
#include <cmath>

#include "Engine/DisplayLayouts/Grid.hpp"
#include "Engine/Math/Vec.hpp"
#include "Engine/Noise/Noise.hpp"

namespace pd::stages {

using math::Vec3;
using math::clampf;
using math::smoothstepf;

namespace {

Vec3 stuckColor(StuckPixelMode mode, float hsel) {
    switch (mode) {
        case StuckPixelMode::Green: return {0, 1, 0};
        case StuckPixelMode::Blue:  return {0, 0, 1};
        case StuckPixelMode::Red:   return {1, 0, 0};
        case StuckPixelMode::White: return {1, 1, 1};
        case StuckPixelMode::RandomRGB:
        default: {
            int c = static_cast<int>(hsel * 3.0f) % 3;
            return c == 0 ? Vec3{1, 0, 0} : (c == 1 ? Vec3{0, 1, 0} : Vec3{0, 0, 1});
        }
    }
}

}  // namespace

void ImperfectionsStage::executeCpu(RenderContext& ctx) const {
    const ParamSnapshot& snap = ctx.params();
    const ArtifactParams& a = snap.artifacts;
    const DisplayCharacteristics& ch = snap.characteristics;
    const WorkingImage& srcImg = ctx.front();
    WorkingImage& dstImg = ctx.back();

    const int w = srcImg.width();
    const int h = srcImg.height();
    if (w == 0 || h == 0) return;

    const layout::GridMapping grid = layout::GridMapping::from(snap.display, w, h);
    const long numCells = std::max(1L, grid.cellCount(w, h));
    const std::uint32_t seed = a.deadPixelSeed;

    // Per-cell defect densities (statistical; approximately `count` cells).
    const float deadProb  = clampf(static_cast<float>(a.deadPixelCount) / numCells, 0.0f, 1.0f);
    const float stuckProb = clampf(static_cast<float>(a.stuckPixelCount) / numCells, 0.0f, 1.0f);
    const float hotProb   = clampf(static_cast<float>(a.hotPixelCount) / numCells, 0.0f, 1.0f);

    const float invW = 1.0f / w, invH = 1.0f / h;

    for (int y = 0; y < h; ++y) {
        const RGBA* in = srcImg.row(y);
        RGBA* out = dstImg.row(y);
        for (int x = 0; x < w; ++x) {
            Vec3 c{in[x].r, in[x].g, in[x].b};
            float alpha = in[x].a;

            const float nx = (x + 0.5f) * invW - 0.5f;   // [-0.5, 0.5]
            const float ny = (y + 0.5f) * invH - 0.5f;
            const float len = std::sqrt(nx * nx + ny * ny);
            const float rNorm = std::min(len / 0.70710678f, 1.0f);  // 0 center .. 1 corner
            const float edge = std::min(std::max(std::fabs(nx), std::fabs(ny)) * 2.0f, 1.0f);

            float mult = 1.0f;

            // --- Smooth non-uniformity fields -------------------------------
            if (a.panelUniformity > 0.0f)
                mult *= 1.0f + a.panelUniformity * 0.5f *
                               noise::fbmSigned(nx * 3.0f + 5, ny * 3.0f + 5, seed + 11u, 3);
            if (a.mura > 0.0f)
                mult *= 1.0f + a.mura * 0.6f *
                               noise::fbmSigned(nx * 9.0f, ny * 9.0f, seed + 22u, 4);
            if (a.brightnessDrift > 0.0f)
                mult *= 1.0f + a.brightnessDrift * 0.5f * nx * 2.0f;  // left→right drift

            // --- Column / row defects ---------------------------------------
            if (a.columnDefects > 0.0f) {
                int cx, cy; grid.cell(x, y, cx, cy);
                if (noise::hash2f(cx, 777, seed) < a.columnDefects * 0.15f)
                    mult *= 0.2f + 0.6f * noise::hash2f(cx, 778, seed);
            }
            if (a.rowDefects > 0.0f) {
                int cx, cy; grid.cell(x, y, cx, cy);
                if (noise::hash2f(999, cy, seed) < a.rowDefects * 0.15f)
                    mult *= 0.2f + 0.6f * noise::hash2f(998, cy, seed);
            }

            c = c * std::max(mult, 0.0f);

            // --- Vignetting -------------------------------------------------
            if (a.vignetting > 0.0f)
                c = c * (1.0f - a.vignetting * smoothstepf(0.35f, 1.0f, rNorm));

            // --- Backlight bleed (LCD): light seeps in from the edges -------
            if (ch.backlightBleed > 0.0f) {
                float b = ch.backlightBleed * smoothstepf(0.6f, 1.0f, edge);
                c = c + Vec3{b, b, b * 1.15f};  // slightly cool
            }

            // --- Light leakage: coloured leak concentrated at the corners ---
            if (a.lightLeakage > 0.0f) {
                float leak = a.lightLeakage * std::pow(rNorm, 3.0f) * (0.5f + 0.5f * edge);
                c = c + Vec3{leak * 1.1f, leak * 0.7f, leak * 0.4f};  // warm leak
            }

            // --- Black level: panel cannot emit true black ------------------
            if (ch.blackLevel > 0.0f) {
                c.x = std::max(c.x, ch.blackLevel);
                c.y = std::max(c.y, ch.blackLevel);
                c.z = std::max(c.z, ch.blackLevel);
            }

            // --- Dirty screen -----------------------------------------------
            if (a.dust > 0.0f) {
                if (noise::hash2f(x / 3, y / 3, seed ^ 0xD057u) < a.dust * 0.03f)
                    c = c * 0.25f;                          // dark dust speck
            }
            if (a.fingerprints > 0.0f) {
                float smudge = noise::fbm(nx * 5.0f, ny * 5.0f, seed ^ 0xF19u, 3);
                float f = a.fingerprints * std::max(0.0f, smudge - 0.5f) * 2.0f;
                c = c * (1.0f - 0.3f * f) + Vec3{0.04f, 0.04f, 0.05f} * f;  // hazy smudge
            }
            if (a.microScratches > 0.0f) {
                float s = noise::valueNoise(nx * 60.0f + ny * 8.0f, ny * 2.0f, seed ^ 0x5C4au);
                if (s > 1.0f - a.microScratches * 0.06f)
                    c = c + Vec3{0.15f, 0.15f, 0.15f};      // faint bright scratch
            }
            if (a.hair > 0.0f) {
                float hf = noise::fbm(nx * 3.0f, ny * 40.0f, seed ^ 0x0BA1u, 2);
                if (std::fabs(hf - 0.5f) < a.hair * 0.02f)
                    c = c * 0.35f;                          // thin dark hair
            }
            if (a.pressureMarks > 0.0f) {
                float pm = noise::fbm(nx * 2.5f, ny * 2.5f, seed ^ 0x9A3u, 3);
                if (pm > 0.7f) {
                    float t = a.pressureMarks * (pm - 0.7f);
                    c = c * Vec3{1.0f - 0.2f * t, 1.0f + 0.1f * t, 1.0f - 0.1f * t};  // discolour
                }
            }

            // --- Banding: limited effective bit depth -----------------------
            if (a.banding > 0.0f) {
                float levels = 255.0f * (1.0f - a.banding) + 6.0f * a.banding;
                levels = std::max(levels, 2.0f);
                auto q = [levels](float v) {
                    return std::round(clampf(v, 0.0f, 1.0f) * levels) / levels;
                };
                c = {q(c.x), q(c.y), q(c.z)};
            }

            // --- Per-cell dead / stuck / hot pixels -------------------------
            if (deadProb > 0.0f || stuckProb > 0.0f || hotProb > 0.0f) {
                int cx, cy; grid.cell(x, y, cx, cy);
                bool dead = noise::hash2f(cx, cy, seed) < deadProb;
                if (a.deadPixelClusters && !dead)
                    dead = noise::hash2f(cx >> 1, cy >> 1, seed ^ 0xC1u) < deadProb * 0.6f;
                if (dead) {
                    c = Vec3{a.deadPixelColor.x, a.deadPixelColor.y, a.deadPixelColor.z} *
                        a.deadPixelBrightness;
                } else if (noise::hash2f(cx, cy, seed ^ 0xAAAAu) < stuckProb) {
                    c = stuckColor(a.stuckPixelMode, noise::hash2f(cx, cy, seed ^ 0xBEEFu));
                } else if (noise::hash2f(cx, cy, seed ^ 0x5555u) < hotProb) {
                    c = Vec3{1.4f, 1.4f, 1.4f};   // hot pixel: over-bright white
                }
            }

            out[x] = {c.x, c.y, c.z, alpha};
        }
    }
}

}  // namespace pd::stages
