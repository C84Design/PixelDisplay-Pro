// PixelDisplay Pro — Tests/test_optics.cpp
//
// Milestone 8 acceptance: lens/camera optics — glow/bloom spread, defocus blur,
// geometric distortion, and MFR determinism.
#include <cstdint>
#include <thread>
#include <vector>

#include "Engine/Core/Engine.hpp"
#include "Engine/Core/PixelAccess.hpp"
#include "Tests/TestFramework.hpp"

using namespace pd;

namespace {
struct Image { std::vector<std::uint8_t> buf; ImageView view; };
Image make(int w,int h){Image i;i.buf.assign((std::size_t)w*h*4,0);
    i.view={i.buf.data(),w,h,(std::ptrdiff_t)w*4,PixelFormat::ARGB8,ColorSpace::sRGB};return i;}
void set(Image&im,int x,int y,std::uint8_t r,std::uint8_t g,std::uint8_t b){auto*p=&im.buf[((std::size_t)y*im.view.width+x)*4];p[0]=255;p[1]=r;p[2]=g;p[3]=b;}
Image blank(const ImageView&v){Image i;i.buf.assign((std::size_t)v.width*v.height*4,0);i.view=v;i.view.data=i.buf.data();return i;}
ParamSnapshot base(){ParamSnapshot p;p.display.enable=true;p.displayType=DisplayType::Square;
    p.display.pixelSize=1.0f;p.display.spacing=0.0f;p.display.dotSize=1.5f;p.subpixel.enable=false;return p;}
}  // namespace

PD_TEST("pixel glow spreads light onto neighbours") {
    auto eng=Engine::create({BackendPreference::ForceCpu,0}).value();
    auto in=make(48,48);
    for(int y=22;y<26;++y)for(int x=22;x<26;++x) set(in,x,y,255,255,255);  // bright block on black
    auto out=blank(in.view);
    auto p=base(); p.characteristics.glowRadius=6.0f; p.characteristics.glowIntensity=1.0f;
    RenderRequest req{in.view,out.view,&p,{},RenderQuality::Final,{},nullptr};
    PD_CHECK(eng->render(req).ok());
    RGBA near=loadPixel(out.view, 30, 24);   // several px from the block edge
    PD_CHECK(near.r > 0.02f);                // received glow
}

PD_TEST("glow off leaves the black region black") {
    auto eng=Engine::create({BackendPreference::ForceCpu,0}).value();
    auto in=make(48,48);
    for(int y=22;y<26;++y)for(int x=22;x<26;++x) set(in,x,y,255,255,255);
    auto out=blank(in.view);
    auto p=base();  // no optics
    RenderRequest req{in.view,out.view,&p,{},RenderQuality::Final,{},nullptr};
    PD_CHECK(eng->render(req).ok());
    RGBA near=loadPixel(out.view, 30, 24);
    PD_CHECK(near.r < 0.01f);                // no spread without glow
}

PD_TEST("camera defocus blurs a hard edge") {
    auto eng=Engine::create({BackendPreference::ForceCpu,0}).value();
    const int w=64,h=16; auto in=make(w,h);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){std::uint8_t v=(x>=w/2)?255:0;set(in,x,y,v,v,v);}
    auto out=blank(in.view);
    auto p=base(); p.lens.cameraDefocus=5.0f;
    RenderRequest req{in.view,out.view,&p,{},RenderQuality::Final,{},nullptr};
    PD_CHECK(eng->render(req).ok());
    // A few px left of the edge (was pure black) now has light bled across.
    RGBA c=loadPixel(out.view, w/2-3, h/2);
    PD_CHECK(c.r > 0.05f);
    PD_CHECK(c.r < 0.95f);
}

PD_TEST("screen curvature changes the image") {
    auto eng=Engine::create({BackendPreference::ForceCpu,0}).value();
    const int w=64,h=64; auto in=make(w,h);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) set(in,x,y,(std::uint8_t)(x*4),(std::uint8_t)(y*4),128);
    auto flat=blank(in.view), curved=blank(in.view);
    auto p=base();
    RenderRequest r0{in.view,flat.view,&p,{},RenderQuality::Final,{},nullptr};
    (void)eng->render(r0);
    p.lens.screenCurvature=0.9f;
    RenderRequest r1{in.view,curved.view,&p,{},RenderQuality::Final,{},nullptr};
    (void)eng->render(r1);
    PD_CHECK(flat.buf != curved.buf);
}

PD_TEST("optics stack is MFR-safe") {
    auto eng=Engine::create({BackendPreference::ForceCpu,0}).value();
    auto in=make(64,64);
    for(int y=0;y<64;++y)for(int x=0;x<64;++x) set(in,x,y,(std::uint8_t)(x*4),200,(std::uint8_t)(y*4));
    auto p=base();
    p.characteristics.glowRadius=5; p.characteristics.glowIntensity=0.8f;
    p.characteristics.bloomIntensity=0.6f; p.characteristics.bloomThreshold=0.3f;
    p.lens.chromaticAberration=4; p.lens.screenCurvature=0.5f; p.lens.moire=0.4f;
    auto ref=blank(in.view);
    RenderRequest r0{in.view,ref.view,&p,{},RenderQuality::Final,{},nullptr};
    PD_CHECK(eng->render(r0).ok());
    std::vector<Image> outs; for(int i=0;i<8;++i) outs.push_back(blank(in.view));
    std::vector<std::thread> th;
    for(int i=0;i<8;++i) th.emplace_back([&,i]{RenderRequest r{in.view,outs[i].view,&p,{},RenderQuality::Final,{},nullptr};(void)eng->render(r);});
    for(auto&x:th)x.join();
    for(int i=0;i<8;++i) PD_CHECK(outs[i].buf==ref.buf);
}
