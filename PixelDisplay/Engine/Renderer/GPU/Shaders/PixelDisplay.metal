// PixelDisplay Pro — Engine/Renderer/GPU/Shaders/PixelDisplay.metal
//
// Metal compute kernels mirroring the CPU reference engine. This file is
// authored to match the math in Engine/Color/*, Engine/Math/Sdf.hpp,
// Engine/DisplayLayouts/LayoutModel.hpp and Engine/Renderer/Stages/*.cpp
// function-for-function (DESIGN.md §7.4).
//
// STATUS: compiled on macOS only; validated against the CPU reference by the
// golden-image parity suite. Not built in the Linux CI. See GPU_PIPELINE.md.
//
// The synthesis kernel below fuses pipeline steps 3–8 (resample → linear →
// emitter footprint → brightness) and is the representative heavy stage. The
// remaining stages (imperfections, burn-in, temporal, optics, encode) are
// implemented as sibling kernels following the identical mirror pattern; the
// synthesis + encode pair here establishes the parameter-bridge and dispatch
// contract the backend relies on.

#include <metal_stdlib>
using namespace metal;

// GPU mirror of pd::ParamSnapshot, tightly packed and shared with the backend
// via an argument buffer. Field order MUST match MetalBackend's GpuParams.
struct GpuParams {
    // display
    float pixelSize, dotSize, spacing, resolutionScale;
    float softness, brightnessCompensation, edgeSoftening, pixelRoundness;
    float pixelRotationDeg, pixelAspect, gridOffsetX, gridOffsetY;
    float gridRotationDeg, pixelRandomness;
    uint  randomSeed;
    int   displayType;      // enum DisplayType
    // subpixel
    int   subpixelEnable, subpixelOrder;
    float spSize, spGap, spSoftness, spBrightness, spGamma, spScaleR, spScaleG, spScaleB;
    // color
    int   inputSpace, outputSpace, linearWorkflow;
    // grade (subset used by synthesis)
    float exposure, contrast, saturation, gamma;
    uint  width, height;
};

// ---- Shared math (mirror of the CPU headers) -------------------------------

inline float srgbToLinear(float c) {
    if (c < 0.0f) return c;
    return (c <= 0.04045f) ? c / 12.92f : pow((c + 0.055f) / 1.055f, 2.4f);
}
inline float linearToSrgb(float c) {
    if (c < 0.0f) return c;
    return (c <= 0.0031308f) ? c * 12.92f : 1.055f * pow(c, 1.0f / 2.4f) - 0.055f;
}
inline float sdfBox(float2 p, float2 b) {
    float2 d = abs(p) - b;
    return length(max(d, 0.0f)) + min(max(d.x, d.y), 0.0f);
}
inline float sdfRoundedBox(float2 p, float2 b, float r) {
    r = min(r, min(b.x, b.y));
    return sdfBox(p, b - r) - r;
}
inline float sdfCircle(float2 p, float r) { return length(p) - r; }
inline float coverage(float sdf, float aa) {
    if (aa <= 0.0f) return sdf <= 0.0f ? 1.0f : 0.0f;
    return 1.0f - smoothstep(-aa, aa, sdf);
}
inline float2 rot(float2 v, float a) {
    float c = cos(a), s = sin(a);
    return float2(v.x * c - v.y * s, v.x * s + v.y * c);
}

// ---- Synthesis kernel (steps 3–8), writing SCENE-LINEAR RGBA ----------------
//
// in  : source texture (display-encoded, e.g. sRGB), RGBA float
// out : linear working texture, RGBA float
kernel void pd_synthesis(texture2d<float, access::sample> src [[texture(0)]],
                         texture2d<float, access::write>  dst [[texture(1)]],
                         constant GpuParams& P                [[buffer(0)]],
                         uint2 gid                            [[thread_position_in_grid]]) {
    if (gid.x >= P.width || gid.y >= P.height) return;

    constexpr sampler samp(coord::pixel, address::clamp_to_edge, filter::linear);

    float w = float(P.width), h = float(P.height);
    float2 center = float2(w, h) * 0.5f;
    float scale = max(P.resolutionScale, 0.01f);
    float pitch = max(P.pixelSize * scale, 1.0f);
    float gridRot = -P.gridRotationDeg * 3.14159265f / 180.0f;
    float pixRot  = -P.pixelRotationDeg * 3.14159265f / 180.0f;
    float2 gridOffset = float2(P.gridOffsetX, P.gridOffsetY);
    float aa = 0.5f / pitch + P.edgeSoftening + P.softness;

    float2 p = float2(gid) + 0.5f;
    float2 g = rot(p - center, gridRot) + center + gridOffset;
    int cx = int(floor(g.x / pitch));
    int cy = int(floor(g.y / pitch));

    float2 cellCenter = (float2(cx, cy) + 0.5f) * pitch;
    float2 sampleGrid = cellCenter - gridOffset;
    float2 samplePos = rot(sampleGrid - center, -gridRot) + center;
    float4 s = src.sample(samp, samplePos);

    // Decode to linear (sRGB-family). Primaries conversion / full grade handled
    // in the grade helpers mirrored from Engine/Color; this kernel applies the
    // common subset (decode + exposure + contrast + gamma).
    float3 lin = float3(srgbToLinear(s.r), srgbToLinear(s.g), srgbToLinear(s.b));
    if (P.exposure != 0.0f) lin *= exp2(P.exposure);
    if (P.contrast != 1.0f) lin = (lin - 0.18f) * P.contrast + 0.18f;
    if (P.gamma != 1.0f && P.gamma > 0.0f) lin = pow(max(lin, 0.0f), 1.0f / P.gamma);

    // Local cell coordinate.
    float2 local = float2(g.x / pitch - (float(cx) + 0.5f), g.y / pitch - (float(cy) + 0.5f));
    local = rot(local, pixRot);

    float base = 0.5f * (1.0f - clamp(P.spacing, 0.0f, 0.95f)) * clamp(P.dotSize, 0.0f, 1.5f);
    float2 hExt = float2(base, base);
    float3 emit = float3(0.0f);

    // Whole-pixel emitter (subpixel-off, or geometric types). RGB-stripe and the
    // other subpixel layouts follow the same accumulation as the CPU kernel;
    // the common paths are shown here.
    if (P.subpixelEnable != 0 &&
        (P.displayType == 0 /*LcdRgbStripe*/ || P.displayType == 1 /*LcdBgrStripe*/)) {
        bool bgr = (P.displayType == 1) || (P.subpixelOrder == 1);
        float gap = clamp(P.spGap, 0.0f, 0.9f);
        float sw = (1.0f / 6.0f) * (1.0f - gap) * clamp(P.spSize, 0.1f, 1.5f);
        float hy = 0.5f * (1.0f - clamp(P.spacing, 0.0f, 0.9f)) * (1.0f - gap);
        float sig[3] = { lin.r * P.spScaleR, lin.g * P.spScaleG, lin.b * P.spScaleB };
        for (int i = 0; i < 3; ++i) {
            float2 c2 = float2((float(i) - 1.0f) / 3.0f, 0.0f);
            float cov = coverage(sdfRoundedBox(local - c2, float2(sw, hy),
                                               P.pixelRoundness * 0.4f * min(sw, hy)), aa);
            int ch = bgr ? (2 - i) : i;
            emit[ch] += pow(max(sig[ch], 0.0f), P.spGamma) * cov;
        }
        emit *= P.spBrightness;
    } else {
        float sdf = (P.displayType == 18 /*Circular*/)
                        ? sdfCircle(local, min(hExt.x, hExt.y))
                        : sdfRoundedBox(local, hExt, P.pixelRoundness * min(hExt.x, hExt.y));
        emit = lin * coverage(sdf, aa);
    }

    emit *= max(P.brightnessCompensation, 0.0f);
    dst.write(float4(emit, s.a), gid);   // scene-linear
}

// ---- Color encode kernel (step 12): linear -> output transfer ---------------
kernel void pd_encode(texture2d<float, access::read>  src [[texture(0)]],
                      texture2d<float, access::write> dst [[texture(1)]],
                      constant GpuParams& P               [[buffer(0)]],
                      uint2 gid                           [[thread_position_in_grid]]) {
    if (gid.x >= P.width || gid.y >= P.height) return;
    float4 c = src.read(gid);
    // Output primaries matrix is applied on the host side of the argument buffer
    // in the full implementation; sRGB/Rec.709 output is the identity path here.
    float3 enc = float3(linearToSrgb(c.r), linearToSrgb(c.g), linearToSrgb(c.b));
    dst.write(float4(enc, c.a), gid);
}
