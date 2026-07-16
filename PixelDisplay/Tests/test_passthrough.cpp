// PixelDisplay Pro — Tests/test_passthrough.cpp
//
// Milestone 1 acceptance test: with the effect disabled (passthrough), the
// engine must reproduce the source exactly, and it must do so identically when
// invoked concurrently from many threads (the MFR contract, DESIGN.md §10).
#include <cstdint>
#include <thread>
#include <vector>

#include "Engine/Core/Engine.hpp"
#include "Tests/TestFramework.hpp"

using namespace pd;

namespace {

// Build a deterministic test image in a given format.
std::vector<std::uint8_t> makeImage(std::int32_t w, std::int32_t h, PixelFormat fmt,
                                    ImageView& view) {
    const std::size_t bpp = bytesPerPixel(fmt);
    std::vector<std::uint8_t> buf(static_cast<std::size_t>(w) * h * bpp);
    for (std::size_t i = 0; i < buf.size(); ++i)
        buf[i] = static_cast<std::uint8_t>((i * 37 + 11) & 0xFF);
    view.data = buf.data();
    view.width = w;
    view.height = h;
    view.rowBytes = static_cast<std::ptrdiff_t>(w) * bpp;
    view.format = fmt;
    view.colorSpace = ColorSpace::sRGB;
    return buf;
}

}  // namespace

PD_TEST("engine constructs with CPU backend") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0});
    PD_CHECK(eng.ok());
    PD_CHECK_EQ(std::string(eng.value()->activeBackendName()), std::string("CPU"));
}

PD_TEST("passthrough reproduces source exactly (ARGB8)") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();

    ImageView in;
    auto src = makeImage(64, 48, PixelFormat::ARGB8, in);
    std::vector<std::uint8_t> dstBuf(src.size(), 0);
    ImageView out = in;
    out.data = dstBuf.data();

    ParamSnapshot params;  // display.enable == false => passthrough
    PD_CHECK(params.isPassthrough());

    RenderRequest req;
    req.input = in;
    req.output = out;
    req.params = &params;

    auto st = eng->render(req);
    PD_CHECK(st.ok());
    PD_CHECK(src == dstBuf);
}

PD_TEST("passthrough works for 16-bit and float formats") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();
    for (PixelFormat fmt : {PixelFormat::RGBA16, PixelFormat::RGBA32F, PixelFormat::ARGB16}) {
        ImageView in;
        auto src = makeImage(33, 17, fmt, in);
        std::vector<std::uint8_t> dstBuf(src.size(), 0xAB);
        ImageView out = in;
        out.data = dstBuf.data();

        ParamSnapshot params;
        RenderRequest req;
        req.input = in;
        req.output = out;
        req.params = &params;

        PD_CHECK(eng->render(req).ok());
        PD_CHECK(src == dstBuf);
    }
}

PD_TEST("concurrent renders are independent and deterministic (MFR)") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();

    ImageView in;
    auto src = makeImage(80, 60, PixelFormat::ARGB8, in);
    ParamSnapshot params;

    constexpr int kThreads = 8;
    std::vector<std::vector<std::uint8_t>> outputs(kThreads,
                                                   std::vector<std::uint8_t>(src.size(), 0));
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t] {
            ImageView out = in;
            out.data = outputs[t].data();
            RenderRequest req;
            req.input = in;
            req.output = out;
            req.params = &params;
            for (int iter = 0; iter < 20; ++iter)
                (void)eng->render(req);
        });
    }
    for (auto& th : threads) th.join();

    for (int t = 0; t < kThreads; ++t)
        PD_CHECK(outputs[t] == src);
}

PD_TEST("stats report the active backend") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();
    ImageView in;
    auto src = makeImage(16, 16, PixelFormat::ARGB8, in);
    std::vector<std::uint8_t> dstBuf(src.size(), 0);
    ImageView out = in;
    out.data = dstBuf.data();

    ParamSnapshot params;
    RenderStats stats;
    RenderRequest req;
    req.input = in;
    req.output = out;
    req.params = &params;
    req.outStats = &stats;

    PD_CHECK(eng->render(req).ok());
    PD_CHECK_EQ(std::string(stats.backend), std::string("CPU"));
    PD_CHECK_EQ(stats.stagesExecuted, 0);
}
