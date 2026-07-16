// PixelDisplay Pro — Engine/Renderer/CPU/CpuBackend.cpp
#include "Engine/Renderer/CPU/CpuBackend.hpp"

#include <cstring>
#include <thread>

#include "Engine/Core/Engine.hpp"       // RenderRequest
#include "Engine/Core/PixelAccess.hpp"

namespace pd::cpu {

namespace {

struct Rect {
    std::int32_t x0, y0, w, h;
};

Rect resolveRegion(const RenderRequest& req) {
    const ImageView& out = req.output;
    if (req.region.full())
        return {0, 0, out.width, out.height};
    // Clamp the requested region to the output bounds.
    std::int32_t x0 = req.region.x < 0 ? 0 : req.region.x;
    std::int32_t y0 = req.region.y < 0 ? 0 : req.region.y;
    std::int32_t w = req.region.width;
    std::int32_t h = req.region.height;
    if (x0 + w > out.width) w = out.width - x0;
    if (y0 + h > out.height) h = out.height - y0;
    return {x0, y0, w, h};
}

// Exact passthrough copy of a region from input to output.
void blit(const ImageView& in, const ImageView& out, const Rect& r, ThreadPool& pool) {
    const bool sameFormat = in.format == out.format;
    const std::size_t bpp = bytesPerPixel(out.format);
    pool.parallelFor(static_cast<std::size_t>(r.h), [&](std::size_t row) {
        std::int32_t y = r.y0 + static_cast<std::int32_t>(row);
        if (sameFormat) {
            const std::uint8_t* src = in.rowPtr(y) + static_cast<std::ptrdiff_t>(r.x0) * bpp;
            std::uint8_t* dst = out.rowPtr(y) + static_cast<std::ptrdiff_t>(r.x0) * bpp;
            std::memcpy(dst, src, static_cast<std::size_t>(r.w) * bpp);
        } else {
            for (std::int32_t x = r.x0; x < r.x0 + r.w; ++x)
                storePixel(out, x, y, [](RGBA c) { return c; }(loadPixel(in, x, y)));
        }
    });
}

}  // namespace

CpuBackend::CpuBackend(unsigned threads) : pool_(threads) {}

BackendCaps CpuBackend::caps() const {
    BackendCaps c;
    c.isGpu = false;
    c.hasFloat16 = false;
    c.hasWaveOps = false;
    c.preferredTile = 64;
    c.memoryBudgetBytes = 0;  // host-managed; engine bounds its own caches
    return c;
}

Status CpuBackend::execute(const FrameGraph& graph, const RenderRequest& req,
                           RenderContext& ctx) {
    if (!req.input.valid() || !req.output.valid())
        return Error(ErrorCode::InvalidArgument, "input/output ImageView is invalid");
    if (req.output.width != req.input.width || req.output.height != req.input.height)
        return Error(ErrorCode::InvalidArgument, "input/output dimensions differ");

    const Rect r = resolveRegion(req);
    if (r.w <= 0 || r.h <= 0) return Status{};  // nothing to do

    // Passthrough fast path: no stages => bit-exact copy.
    if (graph.empty()) {
        blit(req.input, req.output, r, pool_);
        ctx.stats().backend = name();
        ctx.stats().stagesExecuted = 0;
        return Status{};
    }

    // Decode input region into the working buffer (region-local coordinates).
    WorkingImage& front = ctx.front();
    pool_.parallelFor(static_cast<std::size_t>(r.h), [&](std::size_t row) {
        std::int32_t y = static_cast<std::int32_t>(row);
        RGBA* dst = front.row(y);
        for (std::int32_t x = 0; x < r.w; ++x)
            dst[x] = loadPixel(req.input, r.x0 + x, r.y0 + y);
    });

    // Run stages. Each transforms front() -> back(); the executor then swaps so
    // the output of stage N is the input of stage N+1.
    int stageCount = 0;
    for (const auto& stage : graph.stages()) {
        stage->executeCpu(ctx);
        ctx.swap();
        ++stageCount;
    }

    // Encode the final working buffer into the output region.
    const WorkingImage& result = ctx.front();
    pool_.parallelFor(static_cast<std::size_t>(r.h), [&](std::size_t row) {
        std::int32_t y = static_cast<std::int32_t>(row);
        const RGBA* src = result.row(y);
        for (std::int32_t x = 0; x < r.w; ++x)
            storePixel(req.output, r.x0 + x, r.y0 + y, src[x]);
    });

    ctx.stats().backend = name();
    ctx.stats().stagesExecuted = stageCount;
    return Status{};
}

}  // namespace pd::cpu
