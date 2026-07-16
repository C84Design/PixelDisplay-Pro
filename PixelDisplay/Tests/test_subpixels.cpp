// PixelDisplay Pro — Tests/test_subpixels.cpp
//
// Milestone 3 acceptance: cells decompose into channel-specific subpixels. On an
// RGB-stripe panel with a white source, the left/middle/right thirds of a cell
// must show red/green/blue respectively; BGR reverses it; disabling subpixels
// falls back to a full-colour emitter.
#include <cstdint>
#include <vector>

#include "Engine/Core/Engine.hpp"
#include "Engine/Core/PixelAccess.hpp"
#include "Tests/TestFramework.hpp"

using namespace pd;

namespace {
struct Image { std::vector<std::uint8_t> buf; ImageView view; };

Image solid(int w, int h, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    Image img;
    img.buf.resize(static_cast<std::size_t>(w) * h * 4);
    for (std::size_t i = 0; i < img.buf.size(); i += 4) {
        img.buf[i] = 255; img.buf[i+1] = r; img.buf[i+2] = g; img.buf[i+3] = b;
    }
    img.view = {img.buf.data(), w, h, (std::ptrdiff_t)w*4, PixelFormat::ARGB8, ColorSpace::sRGB};
    return img;
}
Image blank(const ImageView& like) {
    Image img; img.buf.assign((std::size_t)like.width*like.height*4, 0);
    img.view = like; img.view.data = img.buf.data(); return img;
}

Image renderPanel(Engine& eng, const ImageView& in, DisplayType type, bool subpixels) {
    Image out = blank(in);
    ParamSnapshot p;
    p.display.enable = true;
    p.displayType = type;
    p.display.pixelSize = 24.0f;
    p.display.spacing = 0.1f;
    p.display.dotSize = 0.95f;
    p.subpixel.enable = subpixels;
    p.subpixel.gap = 0.1f;
    RenderRequest req; req.input = in; req.output = out.view; req.params = &p;
    (void)eng.render(req);
    return out;
}
}  // namespace

PD_TEST("RGB stripe lights R/G/B in left/middle/right thirds") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();
    auto in = solid(96, 96, 255, 255, 255);          // white source
    auto out = renderPanel(*eng, in.view, DisplayType::LcdRgbStripe, true);

    // Cell 0 spans x∈[0,24), center (12,12). Stripe centers: R≈4, G≈12, B≈20.
    RGBA red   = loadPixel(out.view, 4, 12);
    RGBA green = loadPixel(out.view, 12, 12);
    RGBA blue  = loadPixel(out.view, 20, 12);
    PD_CHECK(red.r > red.g && red.r > red.b);
    PD_CHECK(green.g > green.r && green.g > green.b);
    PD_CHECK(blue.b > blue.r && blue.b > blue.g);
}

PD_TEST("BGR stripe reverses the channel order") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();
    auto in = solid(96, 96, 255, 255, 255);
    auto out = renderPanel(*eng, in.view, DisplayType::LcdBgrStripe, true);
    RGBA leftStripe = loadPixel(out.view, 4, 12);   // leftmost => blue for BGR
    PD_CHECK(leftStripe.b > leftStripe.r);
    PD_CHECK(leftStripe.b > leftStripe.g);
}

PD_TEST("green source lights only green subpixels") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();
    auto in = solid(96, 96, 0, 255, 0);              // pure green
    auto out = renderPanel(*eng, in.view, DisplayType::LcdRgbStripe, true);
    RGBA greenStripe = loadPixel(out.view, 12, 12);  // middle stripe
    RGBA redStripe   = loadPixel(out.view, 4, 12);   // left stripe (R): should be dark
    PD_CHECK(greenStripe.g > 0.4f);
    PD_CHECK(redStripe.r < 0.1f);                    // no red signal => red stays off
}

PD_TEST("disabling subpixels falls back to a full-colour emitter") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();
    auto in = solid(96, 96, 255, 255, 255);
    auto out = renderPanel(*eng, in.view, DisplayType::LcdRgbStripe, false);
    RGBA center = loadPixel(out.view, 12, 12);
    // A white source through a full-colour emitter => roughly neutral (not split).
    PD_CHECK(center.r > 0.5f && center.g > 0.5f && center.b > 0.5f);
    PD_CHECK_NEAR(center.r, center.g, 0.05f);
    PD_CHECK_NEAR(center.g, center.b, 0.05f);
}
