// PixelDisplay Pro — Engine/Core/Engine.hpp
//
// The engine facade. This is the entire surface a host adapter uses:
//   auto eng = Engine::create(cfg);
//   eng->render(request);
// It owns the active backend and (from later milestones) the ResourceCache.
// It is thread-safe: render() may be called concurrently from MFR workers.
#pragma once

#include <memory>
#include <mutex>

#include "Engine/Core/FrameGraph.hpp"
#include "Engine/Core/ParamSnapshot.hpp"
#include "Engine/Core/RenderContext.hpp"
#include "Engine/Core/ResourceCache.hpp"
#include "Engine/Core/Types.hpp"
#include "Engine/Renderer/Backend.hpp"
#include "Engine/Utilities/Result.hpp"

namespace pd {

/// One render invocation. Input/output are borrowed host buffers. `params` must
/// outlive the call. This is the POD interface promised in DESIGN.md §2.
struct RenderRequest {
    ImageView input;
    ImageView output;
    const ParamSnapshot* params = nullptr;
    TimeInfo time;
    RenderQuality quality = RenderQuality::Final;
    RenderRegion region;  // full image when default-constructed
    RenderStats* outStats = nullptr;  // optional; filled on success
};

/// Backend selection preference.
enum class BackendPreference {
    Auto,   // Metal > D3D12 > OpenGL > CPU
    ForceCpu,
    ForceGpu,
};

struct EngineConfig {
    BackendPreference preference = BackendPreference::Auto;
    unsigned cpuThreads = 0;  // 0 => hardware_concurrency
};

class Engine {
public:
    /// Construct an engine, selecting and initializing a backend.
    static Result<std::unique_ptr<Engine>> create(const EngineConfig& config = {});

    /// Test/advanced hook: construct with an explicit primary backend and an
    /// optional CPU-reference fallback used on GPU device-loss / unsupported
    /// stages. Ownership is transferred.
    static Result<std::unique_ptr<Engine>> createWithBackends(
        std::unique_ptr<Backend> primary, std::unique_ptr<Backend> fallback);

    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    /// Render one frame. Safe to call concurrently from many threads.
    Status render(const RenderRequest& request);

    const char* activeBackendName() const;
    BackendCaps activeBackendCaps() const;

    /// Number of distinct frame graphs compiled so far (cache misses). Repeated
    /// renders with the same topology reuse a cached graph. For diagnostics.
    std::uint64_t compiledGraphCount() const;

private:
    Engine() = default;

    std::unique_ptr<Backend> backend_;   // primary (may be GPU)
    std::unique_ptr<Backend> fallback_;  // CPU reference used on GPU failure
    ResourceCache cache_;  // memoizes compiled graphs (and, later, GPU resources)
};

}  // namespace pd
