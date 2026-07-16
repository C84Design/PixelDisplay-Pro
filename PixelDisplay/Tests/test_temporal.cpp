// PixelDisplay Pro — Tests/test_temporal.cpp
//
// Milestone 7 acceptance: temporal/animation effects and camera rolling shutter,
// all closed-form in frame time/index.
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
    p.display.pixelSize=1.0f;p.display.spacing=0.0f;p.display.dotSize=1.5f;p.subpixel.enable=false;return p;}
float avg(const ImageView&v){double s=0;int n=0;for(int y=0;y<v.height;++y)for(int x=0;x<v.width;++x){s+=loadPixel(v,x,y).r;++n;}return (float)(s/n);}
}  // namespace

PD_TEST("PWM dims the frame during the off phase") {
    auto eng=Engine::create({BackendPreference::ForceCpu,0}).value();
    auto in=solid(32,32,180);
    auto p=base(); p.animation.pwmFrequency=10.0f; p.animation.pwmDutyCycle=0.5f; p.animation.pwmIntensity=0.8f;

    auto renderAt=[&](double t){auto o=blank(in.view);RenderRequest r{in.view,o.view,&p,{},RenderQuality::Final,{},nullptr};r.time.layerTimeSeconds=t;(void)eng->render(r);return avg(o.view);};
    float on=renderAt(0.0);    // phase 0.0 => lit
    float off=renderAt(0.06);  // phase 0.6 => dimmed
    PD_CHECK(on > off + 0.1f);
}

PD_TEST("rolling shutter produces horizontal banding") {
    auto eng=Engine::create({BackendPreference::ForceCpu,0}).value();
    auto in=solid(16,64,180); auto out=blank(in.view);
    auto p=base();
    p.animation.pwmFrequency=5.0f; p.animation.pwmDutyCycle=0.5f; p.animation.pwmIntensity=1.0f;
    p.lens.rollingShutterEnable=true; p.lens.readoutTimeMs=200.0f;   // ~1 cycle top-to-bottom
    p.lens.sensorDirection=SensorDirection::TopToBottom;
    RenderRequest req{in.view,out.view,&p,{},RenderQuality::Final,{},nullptr};
    req.time.layerTimeSeconds=0.0;
    PD_CHECK(eng->render(req).ok());
    // Rows are captured at different times => some rows lit, some dark.
    float lo=1e9f, hi=-1e9f;
    for(int y=0;y<64;++y){float v=loadPixel(out.view,8,y).r; lo=std::min(lo,v); hi=std::max(hi,v);}
    PD_CHECK(hi - lo > 0.3f);
}

PD_TEST("temporal noise differs between frames but is deterministic per frame") {
    auto eng=Engine::create({BackendPreference::ForceCpu,0}).value();
    auto in=solid(32,32,120);
    auto p=base(); p.animation.temporalNoise=1.0f;
    auto frame=[&](long idx){auto o=blank(in.view);RenderRequest r{in.view,o.view,&p,{},RenderQuality::Final,{},nullptr};r.time.frameIndex=idx;(void)eng->render(r);return o.buf;};
    auto f1=frame(1), f1b=frame(1), f2=frame(2);
    PD_CHECK(f1==f1b);   // same frame index => identical
    PD_CHECK(f1!=f2);    // different frame => different grain
}

PD_TEST("temporal effects are MFR-safe for a fixed frame") {
    auto eng=Engine::create({BackendPreference::ForceCpu,0}).value();
    auto in=solid(48,48,150);
    auto p=base();
    p.animation.pwmFrequency=8; p.animation.pwmIntensity=0.5f; p.animation.scanlineOpacity=0.4f;
    p.animation.temporalNoise=0.5f; p.animation.pixelTwinkle=0.3f;
    auto ref=blank(in.view);
    RenderRequest r0{in.view,ref.view,&p,{},RenderQuality::Final,{},nullptr}; r0.time.frameIndex=7; r0.time.layerTimeSeconds=0.233;
    PD_CHECK(eng->render(r0).ok());
    std::vector<Image> outs; for(int i=0;i<8;++i) outs.push_back(blank(in.view));
    std::vector<std::thread> th;
    for(int i=0;i<8;++i) th.emplace_back([&,i]{RenderRequest r{in.view,outs[i].view,&p,{},RenderQuality::Final,{},nullptr};r.time.frameIndex=7;r.time.layerTimeSeconds=0.233;(void)eng->render(r);});
    for(auto&x:th)x.join();
    for(int i=0;i<8;++i) PD_CHECK(outs[i].buf==ref.buf);
}
