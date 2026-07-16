// PixelDisplay Pro — Engine/Renderer/CPU/CpuBackend.hpp
//
// The always-available reference backend. It is the semantic ground truth for
// golden-image tests and the fallback when no GPU is present or usable. It runs
// stages over the RenderContext's working buffer using a tiled thread pool.
#pragma once

#include "Engine/Renderer/Backend.hpp"
#include "Engine/Renderer/CPU/ThreadPool.hpp"

namespace pd::cpu {

class CpuBackend final : public Backend {
public:
    explicit CpuBackend(unsigned threads = 0);

    const char* name() const override { return "CPU"; }
    bool available() const override { return true; }
    BackendCaps caps() const override;

    Status execute(const FrameGraph& graph, const RenderRequest& request,
                   RenderContext& ctx) override;

private:
    ThreadPool pool_;
};

}  // namespace pd::cpu
