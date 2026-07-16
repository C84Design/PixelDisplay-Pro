// PixelDisplay Pro — Engine/Renderer/Stages/BurnInStage.cpp
#include "Engine/Renderer/Stages/BurnInStage.hpp"

#include <algorithm>
#include <cmath>

#include "Engine/Artifacts/BurnInModel.hpp"
#include "Engine/Math/Vec.hpp"

namespace pd::stages {

using math::Vec3;
using math::clampf;

void BurnInStage::executeCpu(RenderContext& ctx) const {
    const ParamSnapshot& snap = ctx.params();
    const BurnInParams& b = snap.burnIn;
    const DisplayCharacteristics& chr = snap.characteristics;
    const WorkingImage& srcImg = ctx.front();
    WorkingImage& dstImg = ctx.back();

    const int w = srcImg.width();
    const int h = srcImg.height();
    if (w == 0 || h == 0) return;

    const double t = ctx.time().layerTimeSeconds;
    const float grow = artifacts::growth(b.age, t);
    const float recovery = 1.0f - clampf(b.recoverySpeed * 0.2f, 0.0f, 0.8f);
    const float retention = clampf(b.persistence + chr.imagePersistence, 0.0f, 2.0f);
    const bool haveMask = !b.customMask.empty() && b.maskW > 0 && b.maskH > 0;
    const bool havePresets = b.logo || b.statusBar || b.window || b.taskbar;

    const float invW = 1.0f / w, invH = 1.0f / h;

    for (int y = 0; y < h; ++y) {
        const RGBA* in = srcImg.row(y);
        RGBA* out = dstImg.row(y);
        for (int x = 0; x < w; ++x) {
            Vec3 c{in[x].r, in[x].g, in[x].b};
            const float nx = (x + 0.5f) * invW - 0.5f;
            const float ny = (y + 0.5f) * invH - 0.5f;

            // Spatial wear term in [0,1].
            float wear;
            if (haveMask) {
                wear = artifacts::sampleWearMask(b, nx, ny);
            } else if (havePresets) {
                wear = artifacts::presetRegions(b, nx, ny);
            } else {
                // Content proxy: bright static content wears fastest.
                float luma = 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
                wear = clampf(luma, 0.0f, 1.0f);
            }
            wear = clampf(wear, 0.0f, 1.0f);

            const float burn = clampf(b.intensity, 0.0f, 2.0f) * wear * grow * recovery;

            if (burn > 0.0f) {
                // Worn emitters are dimmer (OLED-style differential aging)...
                c = c * (1.0f - 0.6f * burn);
                // ...and a faint ghost of the burned pattern stays visible even
                // on dark content. Ghosting raises its visibility.
                float ghost = burn * (0.10f + 0.25f * clampf(b.ghosting, 0.0f, 1.0f));
                c = c + Vec3{ghost, ghost, ghost};
            }

            // Image persistence / retention: a faint lingering trail of the
            // current bright content (closed-form approximation without history).
            if (retention > 0.0f) {
                float luma = 0.2126f * c.x + 0.7152f * c.y + 0.0722f * c.z;
                float p = retention * 0.08f * luma;
                c = c + Vec3{p, p, p};
            }

            out[x] = {c.x, c.y, c.z, in[x].a};
        }
    }
}

}  // namespace pd::stages
