// PixelDisplay Pro — Engine/Renderer/GPU/Metal/MetalBackend.mm
//
// Metal (macOS) backend. Objective-C++; the ONLY file that imports Metal. The
// engine facade talks to it only through the plain-C++ Backend interface.
//
// STATUS: built and validated on macOS only (see GPU_PIPELINE.md). It dispatches
// the compute kernels in PixelDisplay.metal per graph stage. Stages not yet
// ported to a kernel return ErrorCode::BackendUnavailable so the engine falls
// back to the CPU reference for that render — never producing wrong output.
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "Engine/Renderer/GPU/Metal/MetalBackend.hpp"

#include "Engine/Core/Engine.hpp"  // RenderRequest

namespace pd::metal {

namespace {

// GPU mirror of the parameters the kernels read. Field order MUST match the
// GpuParams struct in PixelDisplay.metal.
struct GpuParams {
    float pixelSize, dotSize, spacing, resolutionScale;
    float softness, brightnessCompensation, edgeSoftening, pixelRoundness;
    float pixelRotationDeg, pixelAspect, gridOffsetX, gridOffsetY;
    float gridRotationDeg, pixelRandomness;
    uint32_t randomSeed;
    int32_t displayType;
    int32_t subpixelEnable, subpixelOrder;
    float spSize, spGap, spSoftness, spBrightness, spGamma, spScaleR, spScaleG, spScaleB;
    int32_t inputSpace, outputSpace, linearWorkflow;
    float exposure, contrast, saturation, gamma;
    uint32_t width, height;
};

GpuParams makeGpuParams(const ParamSnapshot& s, uint32_t w, uint32_t h) {
    GpuParams p{};
    p.pixelSize = s.display.pixelSize; p.dotSize = s.display.dotSize;
    p.spacing = s.display.spacing; p.resolutionScale = s.display.resolutionScale;
    p.softness = s.display.softness; p.brightnessCompensation = s.display.brightnessCompensation;
    p.edgeSoftening = s.display.edgeSoftening; p.pixelRoundness = s.display.pixelRoundness;
    p.pixelRotationDeg = s.display.pixelRotationDeg; p.pixelAspect = s.display.pixelAspect;
    p.gridOffsetX = s.display.gridOffsetX; p.gridOffsetY = s.display.gridOffsetY;
    p.gridRotationDeg = s.display.gridRotationDeg; p.pixelRandomness = s.display.pixelRandomness;
    p.randomSeed = s.display.randomSeed; p.displayType = int32_t(s.displayType);
    p.subpixelEnable = s.subpixel.enable ? 1 : 0; p.subpixelOrder = int32_t(s.subpixel.order);
    p.spSize = s.subpixel.size; p.spGap = s.subpixel.gap; p.spSoftness = s.subpixel.softness;
    p.spBrightness = s.subpixel.brightness; p.spGamma = s.subpixel.gamma;
    p.spScaleR = s.subpixel.scaleR; p.spScaleG = s.subpixel.scaleG; p.spScaleB = s.subpixel.scaleB;
    p.inputSpace = int32_t(s.color.inputSpace); p.outputSpace = int32_t(s.color.outputSpace);
    p.linearWorkflow = s.color.linearWorkflow ? 1 : 0;
    p.exposure = s.color.exposure; p.contrast = s.color.contrast;
    p.saturation = s.color.saturation; p.gamma = s.color.gamma;
    p.width = w; p.height = h;
    return p;
}

class MetalBackend final : public Backend {
public:
    explicit MetalBackend(id<MTLDevice> device) : device_(device) {
        queue_ = [device_ newCommandQueue];
        buildPipelines();
    }

    const char* name() const override { return "Metal"; }
    bool available() const override { return device_ != nil && synthesisPso_ != nil; }

    BackendCaps caps() const override {
        BackendCaps c;
        c.isGpu = true;
        c.hasFloat16 = true;
        c.hasWaveOps = true;
        c.preferredTile = 16;
        c.memoryBudgetBytes = device_ ? static_cast<std::size_t>(device_.recommendedMaxWorkingSetSize) : 0;
        return c;
    }

    Status execute(const FrameGraph& graph, const RenderRequest& req, RenderContext& ctx) override;

private:
    void buildPipelines();
    id<MTLComputePipelineState> pipelineFor(ShaderId id) const;

    id<MTLDevice> device_ = nil;
    id<MTLCommandQueue> queue_ = nil;
    id<MTLComputePipelineState> synthesisPso_ = nil;
    id<MTLComputePipelineState> encodePso_ = nil;
};

void MetalBackend::buildPipelines() {
    @autoreleasepool {
        NSError* err = nil;
        // The .metal source is compiled to a default.metallib at build time and
        // embedded; load the default library from the bundle.
        id<MTLLibrary> lib = [device_ newDefaultLibrary];
        if (!lib) return;
        auto make = [&](NSString* fn) -> id<MTLComputePipelineState> {
            id<MTLFunction> f = [lib newFunctionWithName:fn];
            if (!f) return nil;
            return [device_ newComputePipelineStateWithFunction:f error:&err];
        };
        synthesisPso_ = make(@"pd_synthesis");
        encodePso_ = make(@"pd_encode");
    }
}

id<MTLComputePipelineState> MetalBackend::pipelineFor(ShaderId id) const {
    switch (id) {
        case ShaderId::Synthesis:   return synthesisPso_;
        case ShaderId::ColorEncode: return encodePso_;
        default:                    return nil;  // stage not yet ported
    }
}

Status MetalBackend::execute(const FrameGraph& graph, const RenderRequest& req,
                             RenderContext& ctx) {
    // Any stage without a compiled kernel => defer to the CPU reference.
    for (const auto& stage : graph.stages())
        if (pipelineFor(stage->shaderId()) == nil)
            return Error(ErrorCode::BackendUnavailable,
                         std::string("Metal: stage not yet ported: ") + stage->name());
    if (graph.empty())
        return Error(ErrorCode::BackendUnavailable, "Metal: passthrough handled on CPU");

    @autoreleasepool {
        const uint32_t w = static_cast<uint32_t>(req.output.width);
        const uint32_t h = static_cast<uint32_t>(req.output.height);

        MTLTextureDescriptor* desc =
            [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                                               width:w height:h mipmapped:NO];
        desc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
        desc.storageMode = MTLStorageModeManaged;

        id<MTLTexture> texA = [device_ newTextureWithDescriptor:desc];
        id<MTLTexture> texB = [device_ newTextureWithDescriptor:desc];
        if (!texA || !texB)
            return Error(ErrorCode::OutOfMemory, "Metal: texture allocation failed");

        // Upload source (host ImageView -> RGBA32F texture). Conversion from the
        // host pixel format to float RGBA mirrors Engine/Core/PixelAccess.hpp.
        std::vector<float> staging(static_cast<std::size_t>(w) * h * 4);
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x) {
                RGBA c = loadPixel(req.input, int(x), int(y));
                float* d = &staging[(static_cast<std::size_t>(y) * w + x) * 4];
                d[0] = c.r; d[1] = c.g; d[2] = c.b; d[3] = c.a;
            }
        [texA replaceRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0
                  withBytes:staging.data() bytesPerRow:w * 4 * sizeof(float)];

        GpuParams gp = makeGpuParams(ctx.params(), w, h);
        id<MTLBuffer> pbuf = [device_ newBufferWithBytes:&gp length:sizeof(gp)
                                                 options:MTLResourceStorageModeShared];

        id<MTLCommandBuffer> cmd = [queue_ commandBuffer];
        id<MTLTexture> readTex = texA, writeTex = texB;
        MTLSize tg = MTLSizeMake(16, 16, 1);
        MTLSize grid = MTLSizeMake((w + 15) / 16, (h + 15) / 16, 1);

        for (const auto& stage : graph.stages()) {
            id<MTLComputePipelineState> pso = pipelineFor(stage->shaderId());
            id<MTLComputeCommandEncoder> enc = [cmd computeCommandEncoder];
            [enc setComputePipelineState:pso];
            [enc setTexture:readTex atIndex:0];
            [enc setTexture:writeTex atIndex:1];
            [enc setBuffer:pbuf offset:0 atIndex:0];
            [enc dispatchThreadgroups:grid threadsPerThreadgroup:tg];
            [enc endEncoding];
            std::swap(readTex, writeTex);  // result now in readTex
        }
        [cmd commit];
        [cmd waitUntilCompleted];
        if (cmd.status == MTLCommandBufferStatusError)
            return Error(ErrorCode::DeviceLost, "Metal: command buffer failed");

        // Download readTex -> host output.
        [readTex getBytes:staging.data() bytesPerRow:w * 4 * sizeof(float)
               fromRegion:MTLRegionMake2D(0, 0, w, h) mipmapLevel:0];
        for (uint32_t y = 0; y < h; ++y)
            for (uint32_t x = 0; x < w; ++x) {
                const float* s = &staging[(static_cast<std::size_t>(y) * w + x) * 4];
                storePixel(req.output, int(x), int(y), RGBA{s[0], s[1], s[2], s[3]});
            }

        ctx.stats().backend = name();
        ctx.stats().stagesExecuted = static_cast<int>(graph.stages().size());
    }
    return Status{};
}

}  // namespace

std::unique_ptr<Backend> createMetalBackend() {
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    if (!device) return nullptr;
    auto backend = std::make_unique<MetalBackend>(device);
    if (!backend->available()) return nullptr;
    return backend;
}

}  // namespace pd::metal
