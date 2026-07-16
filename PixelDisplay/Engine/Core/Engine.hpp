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

    ~Engine();
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    /// Render one frame. Safe to call concurrently from many threads.
    Status render(const RenderRequest& request);

    const char* activeBackendName() const;
    BackendCaps activeBackendCaps() const;

private:
    Engine() = default;

    std::unique_ptr<Backend> backend_;
    // FrameGraph compilation is cheap and currently per-call; the ResourceCache
    // (M2+) will memoize compiled graphs and procedural geometry by param hash.
};

}  // namespace pd
