// PixelDisplay Pro — Tests/test_color.cpp
//
// Milestone 4 acceptance: linear working space, transfer functions, primaries
// conversion, colour grade, and chromatic aberration.
#include <cstdint>
#include <vector>

#include "Engine/Color/Grade.hpp"
#include "Engine/Color/Primaries.hpp"
#include "Engine/Color/Transfer.hpp"
#include "Engine/Core/Engine.hpp"
#include "Engine/Core/PixelAccess.hpp"
#include "Tests/TestFramework.hpp"

using namespace pd;
using pd::math::Vec3;

namespace {
struct Image { std::vector<std::uint8_t> buf; ImageView view; };
Image make(int w, int h) {
    Image img; img.buf.assign((std::size_t)w*h*4, 0);
    img.view = {img.buf.data(), w, h, (std::ptrdiff_t)w*4, PixelFormat::ARGB8, ColorSpace::sRGB};
    return img;
}
void setPx(Image& im, int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    auto* p = &im.buf[((std::size_t)y*im.view.width + x)*4];
    p[0]=255; p[1]=r; p[2]=g; p[3]=b;
}
}  // namespace

PD_TEST("sRGB transfer round-trips") {
    for (float v : {0.0f, 0.05f, 0.25f, 0.5f, 0.75f, 1.0f}) {
        float rt = color::linearToSrgb(color::srgbToLinear(v));
        PD_CHECK_NEAR(rt, v, 1e-4f);
    }
    PD_CHECK_NEAR(color::srgbToLinear(0.5f), 0.2140411f, 1e-3f);  // known value
}

PD_TEST("primaries convert and round-trip") {
    // A colour taken to P3 and back must return to itself.
    Vec3 c{0.6f, 0.3f, 0.8f};
    Vec3 toP3 = color::workingToOutput(ColorSpace::DisplayP3) * c;
    Vec3 back = color::inputToWorking(ColorSpace::DisplayP3) * toP3;
    PD_CHECK_NEAR(back.x, c.x, 1e-3f);
    PD_CHECK_NEAR(back.y, c.y, 1e-3f);
    PD_CHECK_NEAR(back.z, c.z, 1e-3f);
    // Rec.709 output is identity.
    Vec3 id = color::workingToOutput(ColorSpace::sRGB) * c;
    PD_CHECK_NEAR(id.x, c.x, 1e-5f);
}

PD_TEST("grade: exposure and saturation behave") {
    ColorParams p;
    p.exposure = 1.0f;                       // +1 stop doubles linear
    Vec3 g = color::applyGrade(Vec3{0.2f, 0.2f, 0.2f}, p);
    PD_CHECK_NEAR(g.x, 0.4f, 1e-4f);

    ColorParams s; s.saturation = 0.0f;      // fully desaturated => grey
    Vec3 gs = color::applyGrade(Vec3{0.8f, 0.2f, 0.1f}, s);
    PD_CHECK_NEAR(gs.x, gs.y, 1e-4f);
    PD_CHECK_NEAR(gs.y, gs.z, 1e-4f);
}

PD_TEST("highlight compression prevents clipping") {
    // A very bright linear value is rolled off toward, but never far above, 1.
    float v = color::highlightRolloff(8.0f, 1.0f);
    PD_CHECK(v < 8.0f);
    PD_CHECK(v > 1.0f);
}

PD_TEST("chromatic aberration separates channels at an edge") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();
    const int w = 64, h = 16;
    auto in = make(w, h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            std::uint8_t v = (x >= w / 2) ? 255 : 0;   // black|white vertical edge
            setPx(in, x, y, v, v, v);
        }
    auto out = make(w, h);

    ParamSnapshot p;
    p.display.enable = true;
    p.displayType = DisplayType::Square;
    p.display.pixelSize = 1.0f;   // 1:1 so we sample the signal directly
    p.display.spacing = 0.0f;
    p.display.dotSize = 1.5f;     // full coverage
    p.subpixel.enable = false;
    p.color.caEnable = true;
    p.color.caDirection = CaDirection::Horizontal;
    p.color.caAmount = 4.0f;      // R shifts +x, B shifts -x

    RenderRequest req; req.input = in.view; req.output = out.view; req.params = &p;
    PD_CHECK(eng->render(req).ok());

    // Just left of the edge: R (sampled from the right) sees white, B sees black.
    RGBA c = loadPixel(out.view, w / 2 - 2, h / 2);
    PD_CHECK(c.r > c.b + 0.2f);   // red fringe
}

PD_TEST("output gamut changes the encoded result") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();
    const int w = 8, h = 8;
    auto in = make(w, h);
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x) setPx(in, x, y, 255, 0, 0);

    auto renderWith = [&](ColorSpace outSpace) {
        auto out = make(w, h);
        ParamSnapshot p;
        p.display.enable = true; p.displayType = DisplayType::Square;
        p.display.pixelSize = 1.0f; p.display.spacing = 0.0f; p.display.dotSize = 1.5f;
        p.subpixel.enable = false;
        p.color.outputSpace = outSpace;
        RenderRequest req; req.input = in.view; req.output = out.view; req.params = &p;
        (void)eng->render(req);
        return out.buf;
    };
    auto srgb = renderWith(ColorSpace::sRGB);
    auto p3 = renderWith(ColorSpace::DisplayP3);
    PD_CHECK(srgb != p3);   // saturated red re-mapped into the P3 gamut differs
}
