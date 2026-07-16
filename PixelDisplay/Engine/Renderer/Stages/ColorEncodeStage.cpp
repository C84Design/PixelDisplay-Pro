// PixelDisplay Pro — Engine/Renderer/Stages/ColorEncodeStage.cpp
#include "Engine/Renderer/Stages/ColorEncodeStage.hpp"

#include "Engine/Color/Primaries.hpp"
#include "Engine/Color/Transfer.hpp"
#include "Engine/Math/Vec.hpp"

namespace pd::stages {

using math::Vec3;

void ColorEncodeStage::executeCpu(RenderContext& ctx) const {
    const ColorParams& col = ctx.params().color;
    const WorkingImage& srcImg = ctx.front();
    WorkingImage& dstImg = ctx.back();

    const int w = srcImg.width();
    const int h = srcImg.height();

    const ColorSpace outSpace = col.linearWorkflow ? col.outputSpace : ColorSpace::Linear;
    const math::Mat3 outMat = color::workingToOutput(outSpace);

    for (int y = 0; y < h; ++y) {
        const RGBA* in = srcImg.row(y);
        RGBA* out = dstImg.row(y);
        for (int x = 0; x < w; ++x) {
            const RGBA& c = in[x];
            Vec3 gamut = outMat * Vec3{c.r, c.g, c.b};
            out[x] = {color::encode(gamut.x, outSpace),
                      color::encode(gamut.y, outSpace),
                      color::encode(gamut.z, outSpace),
                      c.a};
        }
    }
}

}  // namespace pd::stages
