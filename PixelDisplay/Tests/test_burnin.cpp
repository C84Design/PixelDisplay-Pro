// PixelDisplay Pro — Tests/test_burnin.cpp
//
// Milestone 6 acceptance: burn-in is spatial, evolves with time, is driven by
// presets/masks, and stays closed-form (identical for equal time across
// threads — no accumulator).
#include <cstdint>
#include <thread>
#include <vector>

#include "Engine/Core/Engine.hpp"
#include "Engine/Core/PixelAccess.hpp"
#include "Tests/TestFramework.hpp"

using namespace pd;

namespace {
struct Image { std::vector<std::uint8_t> buf; ImageView view; };
Image solid(int w,int h,std::uint8_t v){Image i;i.buf.assign((std::size_t)w*h*4,0);
    for(std::size_t k=0;k<i.buf.size();k+=4){i.buf[k]=255;i.buf[k+1]=v;i.buf[k+2]=v;i.buf[k+3]=v;}
    i.view={i.buf.data(),w,h,(std::ptrdiff_t)w*4,PixelFormat::ARGB8,ColorSpace::sRGB};return i;}
Image blank(const ImageView&v){Image i;i.buf.assign((std::size_t)v.width*v.height*4,0);i.view=v;i.view.data=i.buf.data();return i;}
ParamSnapshot base(){ParamSnapshot p;p.display.enable=true;p.displayType=DisplayType::Square;
    p.display.pixelSize=2.0f;p.display.spacing=0.0f;p.display.dotSize=1.5f;p.subpixel.enable=false;return p;}
}  // namespace

PD_TEST("status-bar burn-in leaves a ghost on black content") {
    auto eng=Engine::create({BackendPreference::ForceCpu,0}).value();
    auto in=solid(64,64,0); auto out=blank(in.view);          // black screen
    auto p=base();
    p.burnIn.enable=true; p.burnIn.statusBar=true; p.burnIn.intensity=1.0f;
    p.burnIn.age=1.0f;
    RenderRequest req{in.view,out.view,&p,{},RenderQuality::Final,{},nullptr};
    req.time.layerTimeSeconds=20.0;
    PD_CHECK(eng->render(req).ok());
    RGBA top=loadPixel(out.view, 32, 2);      // in the top status-bar strip
    RGBA middle=loadPixel(out.view, 32, 32);  // outside any region
    PD_CHECK(top.r > middle.r + 0.05f);       // ghost visible only in the region
}

PD_TEST("burn-in intensifies over time") {
    auto eng=Engine::create({BackendPreference::ForceCpu,0}).value();
    auto in=solid(64,64,0);
    auto p=base();
    p.burnIn.enable=true; p.burnIn.statusBar=true; p.burnIn.intensity=1.0f; p.burnIn.age=0.2f;

    auto ghostAt=[&](double t){
        auto out=blank(in.view);
        RenderRequest req{in.view,out.view,&p,{},RenderQuality::Final,{},nullptr};
        req.time.layerTimeSeconds=t;
        (void)eng->render(req);
        return loadPixel(out.view,32,2).r;
    };
    float early=ghostAt(0.5), late=ghostAt(30.0);
    PD_CHECK(late > early);
    PD_CHECK(early >= 0.0f);
}

PD_TEST("custom mask drives the burned region") {
    auto eng=Engine::create({BackendPreference::ForceCpu,0}).value();
    auto in=solid(64,64,0); auto out=blank(in.view);
    auto p=base();
    p.burnIn.enable=true; p.burnIn.intensity=1.0f; p.burnIn.age=1.0f;
    p.burnIn.maskW=8; p.burnIn.maskH=8;
    p.burnIn.customMask.assign(64, 0.0f);
    for(int y=3;y<=4;++y) for(int x=3;x<=4;++x) p.burnIn.customMask[y*8+x]=1.0f;  // center block
    RenderRequest req{in.view,out.view,&p,{},RenderQuality::Final,{},nullptr};
    req.time.layerTimeSeconds=20.0;
    PD_CHECK(eng->render(req).ok());
    RGBA center=loadPixel(out.view, 32, 32);
    RGBA corner=loadPixel(out.view, 2, 2);
    PD_CHECK(center.r > corner.r + 0.05f);
}

PD_TEST("burn-in is closed-form: same time => identical across threads") {
    auto eng=Engine::create({BackendPreference::ForceCpu,0}).value();
    auto in=solid(96,64,90);
    auto p=base();
    p.burnIn.enable=true; p.burnIn.window=true; p.burnIn.intensity=0.8f;
    p.burnIn.age=0.5f; p.burnIn.ghosting=0.5f;
    auto ref=blank(in.view);
    RenderRequest r0{in.view,ref.view,&p,{},RenderQuality::Final,{},nullptr};
    r0.time.layerTimeSeconds=7.5;
    PD_CHECK(eng->render(r0).ok());
    std::vector<Image> outs; for(int i=0;i<8;++i) outs.push_back(blank(in.view));
    std::vector<std::thread> th;
    for(int i=0;i<8;++i) th.emplace_back([&,i]{
        RenderRequest r{in.view,outs[i].view,&p,{},RenderQuality::Final,{},nullptr};
        r.time.layerTimeSeconds=7.5;
        (void)eng->render(r);
    });
    for(auto&x:th)x.join();
    for(int i=0;i<8;++i) PD_CHECK(outs[i].buf==ref.buf);
}
