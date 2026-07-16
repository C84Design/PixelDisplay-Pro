// PixelDisplay Pro — Engine/Renderer/Backend.hpp
//
// Abstract render backend. Concrete backends (Metal, D3D12, OpenGL, CPU) all
// execute the SAME FrameGraph semantics; only the per-pixel implementation
// differs. The engine probes backends in priority order Metal > D3D12 > OpenGL
// > CPU (DESIGN.md §5.2) and falls back on device loss.
#pragma once

#include <cstddef>
#include <memory>

#include "Engine/Core/FrameGraph.hpp"
#include "Engine/Core/RenderContext.hpp"
#include "Engine/Core/Types.hpp"
#include "Engine/Utilities/Result.hpp"

namespace pd {

struct RenderRequest;  // defined in Engine.hpp

/// Device/queue capabilities used for tuning (threadgroup size, precision).
struct BackendCaps {
    bool isGpu = false;
    bool hasFloat16 = false;
    bool hasWaveOps = false;
    std::size_t memoryBudgetBytes = 0;
    std::uint32_t preferredTile = 64;  // GPU threadgroup / CPU task tile edge
};

class Backend {
public:
    virtual ~Backend() = default;

    virtual const char* name() const = 0;
    virtual bool available() const = 0;
    virtual BackendCaps caps() const = 0;

    /// Execute the compiled graph for one request. Decodes the input into the
    /// context's working buffer, runs stages, and encodes into the output view.
    virtual Status execute(const FrameGraph& graph, const RenderRequest& request,
                           RenderContext& ctx) = 0;
};

}  // namespace pd
