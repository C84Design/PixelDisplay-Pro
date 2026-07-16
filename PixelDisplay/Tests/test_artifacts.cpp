// PixelDisplay Pro — Tests/test_artifacts.cpp
//
// Milestone 5 acceptance: panel imperfections behave and stay deterministic.
#include <cstdint>
#include <set>
#include <thread>
#include <vector>

#include "Engine/Core/Engine.hpp"
#include "Engine/Core/PixelAccess.hpp"
#include "Tests/TestFramework.hpp"

using namespace pd;

namespace {
struct Image { std::vector<std::uint8_t> buf; ImageView view; };
Image solid(int w, int h, std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    Image img; img.buf.resize((std::size_t)w*h*4);
    for (std::size_t i=0;i<img.buf.size();i+=4){img.buf[i]=255;img.buf[i+1]=r;img.buf[i+2]=g;img.buf[i+3]=b;}
    img.view={img.buf.data(),w,h,(std::ptrdiff_t)w*4,PixelFormat::ARGB8,ColorSpace::sRGB};
    return img;
}
Image blank(const ImageView& v){Image i;i.buf.assign((std::size_t)v.width*v.height*4,0);i.view=v;i.view.data=i.buf.data();return i;}
ParamSnapshot base() {
    ParamSnapshot p; p.display.enable=true; p.displayType=DisplayType::Square;
    p.display.pixelSize=4.0f; p.display.spacing=0.0f; p.display.dotSize=1.5f;
    p.subpixel.enable=false; return p;
}
}  // namespace

PD_TEST("vignette darkens the corners") {
    auto eng = Engine::create({BackendPreference::ForceCpu,0}).value();
    auto in = solid(128,128,200,200,200); auto out=blank(in.view);
    auto p = base(); p.artifacts.vignetting=0.8f;
    RenderRequest req{in.view,out.view,&p,{},RenderQuality::Final,{},nullptr};
    PD_CHECK(eng->render(req).ok());
    RGBA corner = loadPixel(out.view, 2, 2);
    RGBA middle = loadPixel(out.view, 64, 64);
    PD_CHECK(corner.r < middle.r);
    PD_CHECK(corner.r < middle.r * 0.8f);
}

PD_TEST("dead pixels create dark cells") {
    auto eng = Engine::create({BackendPreference::ForceCpu,0}).value();
    auto in = solid(128,128,255,255,255); auto out=blank(in.view);
    auto p = base();
    // ~1089 cells at pitch 4 on 128x128; 300 dead => a clear minority.
    p.artifacts.deadPixelCount = 300;
    p.artifacts.deadPixelBrightness = 0.0f;
    RenderRequest req{in.view,out.view,&p,{},RenderQuality::Final,{},nullptr};
    PD_CHECK(eng->render(req).ok());
    int dark = 0, total = 128*128;
    for (int y=0;y<128;++y) for(int x=0;x<128;++x)
        if (loadPixel(out.view,x,y).r < 0.05f) ++dark;
    PD_CHECK(dark > 0);              // some cells died
    PD_CHECK(dark < total);         // but not all
}

PD_TEST("black level raises the floor") {
    auto eng = Engine::create({BackendPreference::ForceCpu,0}).value();
    auto in = solid(64,64,0,0,0); auto out=blank(in.view);
    auto p = base(); p.characteristics.blackLevel = 0.25f;
    RenderRequest req{in.view,out.view,&p,{},RenderQuality::Final,{},nullptr};
    PD_CHECK(eng->render(req).ok());
    RGBA c = loadPixel(out.view, 32, 32);
    PD_CHECK(c.r > 0.3f);   // encode(linear 0.25) ≈ 0.53
}

PD_TEST("banding reduces the number of distinct levels") {
    auto eng = Engine::create({BackendPreference::ForceCpu,0}).value();
    // Horizontal gradient input.
    Image in; in.buf.resize((std::size_t)256*8*4);
    for(int y=0;y<8;++y)for(int x=0;x<256;++x){auto*q=&in.buf[((std::size_t)y*256+x)*4];q[0]=255;q[1]=q[2]=q[3]=(std::uint8_t)x;}
    in.view={in.buf.data(),256,8,(std::ptrdiff_t)256*4,PixelFormat::ARGB8,ColorSpace::sRGB};
    auto out=blank(in.view);
    auto p = base(); p.display.pixelSize=1.0f; p.artifacts.banding=0.9f;
    RenderRequest req{in.view,out.view,&p,{},RenderQuality::Final,{},nullptr};
    PD_CHECK(eng->render(req).ok());
    std::set<int> levels;
    for(int x=0;x<256;++x) levels.insert((int)(loadPixel(out.view,x,4).r*255));
    PD_CHECK(levels.size() < 40);   // strong banding collapses the gradient
}

PD_TEST("artifacts stay deterministic across threads") {
    auto eng = Engine::create({BackendPreference::ForceCpu,0}).value();
    auto in = solid(96,96,180,120,200);
    auto p = base();
    p.artifacts.mura=0.5f; p.artifacts.deadPixelCount=300; p.artifacts.stuckPixelCount=150;
    p.artifacts.vignetting=0.4f; p.artifacts.dust=0.5f; p.artifacts.banding=0.3f;
    auto ref = blank(in.view);
    RenderRequest r0{in.view,ref.view,&p,{},RenderQuality::Final,{},nullptr};
    PD_CHECK(eng->render(r0).ok());

    std::vector<Image> outs; for(int i=0;i<8;++i) outs.push_back(blank(in.view));
    std::vector<std::thread> th;
    for(int t=0;t<8;++t) th.emplace_back([&,t]{
        RenderRequest r{in.view,outs[t].view,&p,{},RenderQuality::Final,{},nullptr};
        (void)eng->render(r);
    });
    for(auto&x:th) x.join();
    for(int t=0;t<8;++t) PD_CHECK(outs[t].buf==ref.buf);
}
