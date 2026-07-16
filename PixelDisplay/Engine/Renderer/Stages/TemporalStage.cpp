// PixelDisplay Pro — Engine/Renderer/Stages/TemporalStage.cpp
#include "Engine/Renderer/Stages/TemporalStage.hpp"

#include <cmath>

#include "Engine/DisplayLayouts/Grid.hpp"
#include "Engine/Math/Vec.hpp"
#include "Engine/Noise/Hash.hpp"

namespace pd::stages {

using math::Vec3;
using math::clampf;
using math::fract;

namespace {

// Shortest wrapped distance between two normalized positions on [0,1).
inline float wrapDist(float a, float b) {
    float d = std::fabs(a - b);
    return std::min(d, 1.0f - d);
}

}  // namespace

void TemporalStage::executeCpu(RenderContext& ctx) const {
    const ParamSnapshot& snap = ctx.params();
    const AnimationParams& an = snap.animation;
    const LensParams& lens = snap.lens;
    const WorkingImage& srcImg = ctx.front();
    WorkingImage& dstImg = ctx.back();

    const int w = srcImg.width();
    const int h = srcImg.height();
    if (w == 0 || h == 0) return;

    const double t = ctx.time().layerTimeSeconds;
    const double fps = ctx.time().frameRate > 0 ? ctx.time().frameRate : 30.0;
    const long frameIndex = ctx.time().frameIndex != 0
                                ? ctx.time().frameIndex
                                : static_cast<long>(std::llround(t * fps));

    const layout::GridMapping grid = layout::GridMapping::from(snap.display, w, h);

    // Global (whole-frame) modulations that don't depend on the sensor row.
    float globalMult = 1.0f;
    if (an.randomFlicker > 0.0f) {
        float f = noise::hash2f(static_cast<int>(frameIndex), 0, 0xF11Cu);
        globalMult *= 1.0f - an.randomFlicker * 0.5f * f;
    }
    if (an.pixelWarmUp > 0.0f) {
        // Panel starts dim and warms to full over ~a second of layer time.
        float warm = 1.0f - std::exp(-static_cast<float>(std::max(t, 0.0)) * 3.0f);
        globalMult *= (1.0f - an.pixelWarmUp) + an.pixelWarmUp * warm;
    }

    const float invW = 1.0f / w, invH = 1.0f / h;
    const bool rolling = lens.rollingShutterEnable && lens.readoutTimeMs > 0.0f;
    const float readout = lens.readoutTimeMs / 1000.0f;

    for (int y = 0; y < h; ++y) {
        const RGBA* in = srcImg.row(y);
        RGBA* out = dstImg.row(y);
        const float ay = (y + 0.5f) * invH;  // 0..1 down the frame

        for (int x = 0; x < w; ++x) {
            Vec3 c{in[x].r, in[x].g, in[x].b};
            const float ax = (x + 0.5f) * invW;

            // Rolling shutter: this pixel is captured at a row/column-dependent
            // time, so time-varying effects below "freeze" into spatial bands.
            double te = t;
            if (rolling) {
                float frac;
                switch (lens.sensorDirection) {
                    case SensorDirection::BottomToTop: frac = 1.0f - ay; break;
                    case SensorDirection::LeftToRight: frac = ax; break;
                    case SensorDirection::RightToLeft: frac = 1.0f - ax; break;
                    case SensorDirection::TopToBottom:
                    default:                           frac = ay; break;
                }
                te = t + frac * readout + lens.rollingShutterOffset;
            }

            float mult = globalMult;

            // PWM brightness flicker (backlight/OLED duty-cycle dimming).
            if (an.pwmIntensity > 0.0f && an.pwmFrequency > 0.0f) {
                float phase = fract(static_cast<float>(te) * an.pwmFrequency);
                float duty = clampf(an.pwmDutyCycle, 0.02f, 1.0f);
                if (phase >= duty) mult *= (1.0f - clampf(an.pwmIntensity, 0.0f, 1.0f));
            }

            // Rolling refresh bar sweeping along the scan axis.
            if (an.rollingRefreshEnable) {
                float axis;
                switch (an.rollingDirection) {
                    case SensorDirection::LeftToRight:
                    case SensorDirection::RightToLeft: axis = ax; break;
                    default:                           axis = ay; break;
                }
                float pos = fract(static_cast<float>(te) * std::max(an.rollingSpeed, 0.0f));
                float d = wrapDist(axis, pos);
                float band = 1.0f - math::smoothstepf(0.0f, 0.12f, d);  // near the bar
                mult *= 1.0f - 0.5f * band;
            }

            // Scanlines: static spatial darkening, optionally drifting in time.
            if (an.scanlineOpacity > 0.0f) {
                float thick = clampf(an.scanlineThickness, 0.05f, 1.0f);
                float period = std::max(2.0f, 4.0f);  // every ~2px pair
                float phase = (y + an.scanlineMovement * static_cast<float>(t) * 10.0f) / period;
                float s = 0.5f + 0.5f * std::cos(phase * 6.2831853f);
                float line = math::smoothstepf(1.0f - thick, 1.0f, s);
                mult *= 1.0f - an.scanlineOpacity * (1.0f - line);
            }

            // Pixel twinkle: per-cell brightness shimmer, changing each frame.
            if (an.pixelTwinkle > 0.0f) {
                int cx, cy; grid.cell(x, y, cx, cy);
                float tw = noise::hash2f(cx, cy + static_cast<int>(frameIndex) * 977, 0x7011u);
                mult *= 1.0f - an.pixelTwinkle * 0.5f * tw;
            }

            c = c * std::max(mult, 0.0f);

            // Temporal noise: additive per-pixel grain that changes each frame.
            if (an.temporalNoise > 0.0f) {
                float n = noise::hash2f(x + static_cast<int>(frameIndex) * 131,
                                        y + static_cast<int>(frameIndex) * 977, 0x715Eu) - 0.5f;
                float g = an.temporalNoise * 0.15f * n;
                c = c + Vec3{g, g, g};
            }

            out[x] = {c.x, c.y, c.z, in[x].a};
        }
    }
}

}  // namespace pd::stages
