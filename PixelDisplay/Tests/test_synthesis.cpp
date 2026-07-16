// PixelDisplay Pro — Tests/test_synthesis.cpp
//
// Milestone 2 acceptance: the synthesis stage produces a real display grid.
// Tests are property-based (structure & invariants) rather than exact-pixel so
// they remain valid as the kernel gains detail in later milestones.
#include <cstdint>
#include <thread>
#include <vector>

#include "Engine/Core/Engine.hpp"
#include "Engine/Core/PixelAccess.hpp"
#include "Tests/TestFramework.hpp"

using namespace pd;

namespace {

struct Image {
    std::vector<std::uint8_t> buf;
    ImageView view;
};

Image solid(std::int32_t w, std::int32_t h, std::uint8_t r, std::uint8_t g,
            std::uint8_t b, std::uint8_t a = 255) {
    Image img;
    img.buf.resize(static_cast<std::size_t>(w) * h * 4);
    for (std::size_t i = 0; i < img.buf.size(); i += 4) {
        img.buf[i + 0] = a;  // ARGB8 => A,R,G,B
        img.buf[i + 1] = r;
        img.buf[i + 2] = g;
        img.buf[i + 3] = b;
    }
    img.view.data = img.buf.data();
    img.view.width = w;
    img.view.height = h;
    img.view.rowBytes = static_cast<std::ptrdiff_t>(w) * 4;
    img.view.format = PixelFormat::ARGB8;
    img.view.colorSpace = ColorSpace::sRGB;
    return img;
}

Image blank(const ImageView& like) {
    Image img;
    img.buf.assign(static_cast<std::size_t>(like.width) * like.height * 4, 0);
    img.view = like;
    img.view.data = img.buf.data();
    return img;
}

}  // namespace

PD_TEST("synthesis changes the image when enabled") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();
    auto in = solid(128, 128, 200, 200, 200);
    auto out = blank(in.view);

    ParamSnapshot p;
    p.display.enable = true;
    p.displayType = DisplayType::Square;
    p.display.pixelSize = 10.0f;
    p.display.spacing = 0.3f;
    p.display.dotSize = 0.8f;

    RenderRequest req;
    req.input = in.view;
    req.output = out.view;
    req.params = &p;
    PD_CHECK(eng->render(req).ok());
    PD_CHECK(in.buf != out.buf);  // effect actually did something
}

PD_TEST("gaps are darker than dot centers") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();
    auto in = solid(160, 160, 255, 255, 255);
    auto out = blank(in.view);

    ParamSnapshot p;
    p.display.enable = true;
    p.displayType = DisplayType::RoundedSquare;
    p.display.pixelSize = 16.0f;   // large cells so a gap pixel is well inside a gap
    p.display.spacing = 0.4f;
    p.display.dotSize = 0.7f;
    p.display.softness = 0.0f;
    p.display.edgeSoftening = 0.0f;
    p.display.gridOffsetX = 0.0f;
    p.display.gridOffsetY = 0.0f;

    RenderRequest req;
    req.input = in.view;
    req.output = out.view;
    req.params = &p;
    PD_CHECK(eng->render(req).ok());

    // Cell centers sit at (8,8), (24,24), ... ; gap corners at (0,0),(16,16),...
    RGBA centerC = loadPixel(out.view, 8, 8);
    RGBA gapC = loadPixel(out.view, 0, 0);
    PD_CHECK(centerC.r > 0.5f);            // lit
    PD_CHECK(gapC.r < centerC.r);          // gap is darker than the dot center
    PD_CHECK(gapC.r < 0.25f);              // and genuinely dark
}

PD_TEST("full-coverage emitter approximately preserves a solid colour") {
    // With a solid input, no spacing and full dot coverage, every cell emits its
    // sampled colour at coverage ~1 => output ~= input (round-trip through
    // linear). This validates the color transfer + emission math end to end.
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();
    auto in = solid(96, 96, 128, 64, 192);
    auto out = blank(in.view);

    ParamSnapshot p;
    p.display.enable = true;
    p.displayType = DisplayType::Square;
    p.display.pixelSize = 8.0f;
    p.display.spacing = 0.0f;
    p.display.dotSize = 1.5f;      // overfill so even corners are covered
    p.display.softness = 0.0f;
    p.display.edgeSoftening = 0.0f;

    RenderRequest req;
    req.input = in.view;
    req.output = out.view;
    req.params = &p;
    PD_CHECK(eng->render(req).ok());

    RGBA c = loadPixel(out.view, 40, 40);
    PD_CHECK_NEAR(c.r, 128.0f / 255.0f, 3.0f / 255.0f);
    PD_CHECK_NEAR(c.g, 64.0f / 255.0f, 3.0f / 255.0f);
    PD_CHECK_NEAR(c.b, 192.0f / 255.0f, 3.0f / 255.0f);
}

PD_TEST("synthesis is deterministic and MFR-safe") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();
    auto in = solid(128, 96, 180, 90, 210);

    ParamSnapshot p;
    p.display.enable = true;
    p.displayType = DisplayType::RoundedSquare;
    p.display.pixelSize = 7.0f;
    p.display.spacing = 0.25f;
    p.display.pixelRandomness = 0.6f;   // exercise the seeded jitter path
    p.display.randomSeed = 42u;

    // Reference render.
    auto ref = blank(in.view);
    RenderRequest req;
    req.input = in.view;
    req.output = ref.view;
    req.params = &p;
    PD_CHECK(eng->render(req).ok());

    // Many concurrent renders must all match the reference exactly.
    constexpr int kThreads = 8;
    std::vector<Image> outs;
    for (int i = 0; i < kThreads; ++i) outs.push_back(blank(in.view));
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            RenderRequest r;
            r.input = in.view;
            r.output = outs[t].view;
            r.params = &p;
            (void)eng->render(r);
        });
    }
    for (auto& th : threads) th.join();
    for (int t = 0; t < kThreads; ++t) PD_CHECK(outs[t].buf == ref.buf);
}
