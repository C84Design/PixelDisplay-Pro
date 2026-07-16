// PixelDisplay Pro — Engine/Core/Engine.cpp
#include "Engine/Core/Engine.hpp"

#include <chrono>

#include "Engine/Renderer/CPU/CpuBackend.hpp"

// GPU backends are compiled in per-platform (gated by CMake). Each exposes a
// factory that returns nullptr when unavailable at runtime.
#if defined(PD_WITH_METAL)
#include "Engine/Renderer/GPU/Metal/MetalBackend.hpp"
#endif
#if defined(PD_WITH_D3D12)
#include "Engine/Renderer/GPU/D3D12/D3D12Backend.hpp"
#endif
#if defined(PD_WITH_OPENGL)
#include "Engine/Renderer/GPU/OpenGL/OpenGLBackend.hpp"
#endif

namespace pd {

namespace {

std::unique_ptr<Backend> tryCreateGpuBackend() {
    // Priority: Metal > D3D12 > OpenGL. Each factory returns nullptr if the
    // platform/device is unavailable, so we fall through cleanly.
#if defined(PD_WITH_METAL)
    if (auto b = metal::createMetalBackend(); b && b->available()) return b;
#endif
#if defined(PD_WITH_D3D12)
    if (auto b = d3d12::createD3D12Backend(); b && b->available()) return b;
#endif
#if defined(PD_WITH_OPENGL)
    if (auto b = gl::createOpenGLBackend(); b && b->available()) return b;
#endif
    return nullptr;
}

}  // namespace

Result<std::unique_ptr<Engine>> Engine::create(const EngineConfig& config) {
    auto engine = std::unique_ptr<Engine>(new Engine());

    switch (config.preference) {
        case BackendPreference::ForceCpu:
            engine->backend_ = std::make_unique<cpu::CpuBackend>(config.cpuThreads);
            break;
        case BackendPreference::ForceGpu:
            engine->backend_ = tryCreateGpuBackend();
            if (!engine->backend_)
                return Error(ErrorCode::BackendUnavailable,
                             "ForceGpu requested but no GPU backend is available");
            break;
        case BackendPreference::Auto:
        default:
            engine->backend_ = tryCreateGpuBackend();
            if (!engine->backend_)
                engine->backend_ = std::make_unique<cpu::CpuBackend>(config.cpuThreads);
            break;
    }

    if (!engine->backend_ || !engine->backend_->available())
        return Error(ErrorCode::BackendUnavailable, "no usable render backend");

    // A GPU primary keeps a CPU reference in reserve for device-loss / stages a
    // GPU backend has not yet implemented (DESIGN.md §6.4).
    if (engine->backend_->caps().isGpu)
        engine->fallback_ = std::make_unique<cpu::CpuBackend>(config.cpuThreads);

    return engine;
}

Result<std::unique_ptr<Engine>> Engine::createWithBackends(
    std::unique_ptr<Backend> primary, std::unique_ptr<Backend> fallback) {
    if (!primary) return Error(ErrorCode::InvalidArgument, "null primary backend");
    auto engine = std::unique_ptr<Engine>(new Engine());
    engine->backend_ = std::move(primary);
    engine->fallback_ = std::move(fallback);
    return engine;
}

Engine::~Engine() = default;

const char* Engine::activeBackendName() const {
    return backend_ ? backend_->name() : "none";
}

BackendCaps Engine::activeBackendCaps() const {
    return backend_ ? backend_->caps() : BackendCaps{};
}

std::uint64_t Engine::compiledGraphCount() const {
    return cache_.compileCount();
}

Status Engine::render(const RenderRequest& request) {
    if (!request.params)
        return Error(ErrorCode::InvalidArgument, "RenderRequest.params is null");
    if (!request.input.valid() || !request.output.valid())
        return Error(ErrorCode::InvalidArgument, "invalid input/output ImageView");

    const auto t0 = std::chrono::steady_clock::now();

    // Fetch (or compile once and cache) the frame graph for this configuration.
    // The graph is immutable and shared; executing it from many threads is safe
    // because stages are stateless.
    std::shared_ptr<const FrameGraph> graph = cache_.graph(*request.params);

    // Region-sized working buffers; each render owns its own context (MFR-safe).
    const RenderRegion& reg = request.region;
    const std::int32_t w = reg.full() ? request.output.width : reg.width;
    const std::int32_t h = reg.full() ? request.output.height : reg.height;

    auto attempt = [&](Backend* b) -> Status {
        RenderContext ctx(*request.params, request.time, request.quality, w, h);
        Status st = b->execute(*graph, request, ctx);
        if (st) {
            const auto t1 = std::chrono::steady_clock::now();
            ctx.stats().milliseconds =
                std::chrono::duration<double, std::milli>(t1 - t0).count();
            if (request.outStats) *request.outStats = ctx.stats();
        }
        return st;
    };

    Status st = attempt(backend_.get());
    // Fall back to the CPU reference on GPU device-loss or an unsupported stage.
    if (!st && fallback_ &&
        (st.error().code == ErrorCode::DeviceLost ||
         st.error().code == ErrorCode::BackendUnavailable)) {
        st = attempt(fallback_.get());
    }
    return st;
}

}  // namespace pd
