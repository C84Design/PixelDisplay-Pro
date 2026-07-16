// PixelDisplay Pro — Tests/test_engine_m9.cpp
//
// Milestone 9 (CPU-verifiable parts): the ResourceCache memoizes compiled frame
// graphs by topology, and a GPU backend that cannot run a graph transparently
// falls back to the CPU reference (the design's device-loss path).
#include <cstdint>
#include <vector>

#include "Engine/Core/Engine.hpp"
#include "Engine/Core/PixelAccess.hpp"
#include "Engine/Renderer/CPU/CpuBackend.hpp"
#include "Tests/TestFramework.hpp"

using namespace pd;

namespace {
struct Image { std::vector<std::uint8_t> buf; ImageView view; };
Image solid(int w,int h,std::uint8_t v){Image i;i.buf.assign((std::size_t)w*h*4,0);
    for(std::size_t k=0;k<i.buf.size();k+=4){i.buf[k]=255;i.buf[k+1]=v;i.buf[k+2]=v;i.buf[k+3]=v;}
    i.view={i.buf.data(),w,h,(std::ptrdiff_t)w*4,PixelFormat::ARGB8,ColorSpace::sRGB};return i;}
Image blank(const ImageView&v){Image i;i.buf.assign((std::size_t)v.width*v.height*4,0);i.view=v;i.view.data=i.buf.data();return i;}

// A backend that pretends to be a GPU but always fails — exercises fallback.
class FailingGpuBackend final : public Backend {
public:
    const char* name() const override { return "FailingGPU"; }
    bool available() const override { return true; }
    BackendCaps caps() const override { BackendCaps c; c.isGpu = true; return c; }
    Status execute(const FrameGraph&, const RenderRequest&, RenderContext&) override {
        ++calls;
        return Error(ErrorCode::DeviceLost, "simulated device loss");
    }
    mutable int calls = 0;
};
}  // namespace

PD_TEST("resource cache compiles each topology once") {
    auto eng = Engine::create({BackendPreference::ForceCpu, 0}).value();
    auto in = solid(32,32,128); auto out = blank(in.view);

    ParamSnapshot p; p.display.enable = true; p.displayType = DisplayType::Square;
    RenderRequest req{in.view,out.view,&p,{},RenderQuality::Final,{},nullptr};

    for (int i = 0; i < 5; ++i) { req.time.frameIndex = i; (void)eng->render(req); }
    PD_CHECK_EQ(eng->compiledGraphCount(), std::uint64_t(1));  // same topology reused

    // A different topology (enable an artifact) compiles a second graph.
    p.artifacts.vignetting = 0.5f;
    (void)eng->render(req);
    PD_CHECK_EQ(eng->compiledGraphCount(), std::uint64_t(2));

    // Reverting reuses the first cached graph — no new compile.
    p.artifacts.vignetting = 0.0f;
    (void)eng->render(req);
    PD_CHECK_EQ(eng->compiledGraphCount(), std::uint64_t(2));
}

PD_TEST("GPU failure falls back to the CPU reference") {
    auto failing = std::make_unique<FailingGpuBackend>();
    FailingGpuBackend* raw = failing.get();
    auto eng = Engine::createWithBackends(std::move(failing),
                                          std::make_unique<cpu::CpuBackend>(0)).value();

    auto in = solid(48,48,160); auto out = blank(in.view);
    ParamSnapshot p; p.display.enable = true; p.displayType = DisplayType::LcdRgbStripe;
    p.display.pixelSize = 4.0f;
    RenderStats stats;
    RenderRequest req{in.view,out.view,&p,{},RenderQuality::Final,{},&stats};

    PD_CHECK(eng->render(req).ok());          // succeeded despite GPU failure
    PD_CHECK(raw->calls == 1);                // GPU was attempted
    PD_CHECK_EQ(std::string(stats.backend), std::string("CPU"));  // CPU produced it

    // Result must match a pure-CPU render of the same request.
    auto cpuEng = Engine::create({BackendPreference::ForceCpu, 0}).value();
    auto ref = blank(in.view);
    RenderRequest r2{in.view,ref.view,&p,{},RenderQuality::Final,{},nullptr};
    PD_CHECK(cpuEng->render(r2).ok());
    PD_CHECK(out.buf == ref.buf);
}
