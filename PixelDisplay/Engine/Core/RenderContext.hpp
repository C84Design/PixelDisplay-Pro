// PixelDisplay Pro — Engine/Core/RenderContext.hpp
//
// Per-render, thread-local scratch state. A fresh RenderContext is created for
// each render() call, so it carries NO cross-frame or cross-thread state — this
// is a cornerstone of the Multi-Frame-Rendering contract (DESIGN.md §10).
#pragma once

#include <cstdint>

#include "Engine/Core/ParamSnapshot.hpp"
#include "Engine/Core/Types.hpp"
#include "Engine/Core/WorkingImage.hpp"

namespace pd {

/// Statistics surfaced back to the host's Performance group.
struct RenderStats {
    double milliseconds = 0.0;
    std::size_t peakWorkingBytes = 0;
    std::int32_t stagesExecuted = 0;
    const char* backend = "";
};

/// Read-only view of everything a stage needs plus a ping/pong working pair.
class RenderContext {
public:
    RenderContext(const ParamSnapshot& params, TimeInfo time, RenderQuality quality,
                  std::int32_t width, std::int32_t height)
        : params_(params), time_(time), quality_(quality) {
        front_.resize(width, height);
        back_.resize(width, height);
    }

    const ParamSnapshot& params() const { return params_; }
    const TimeInfo& time() const { return time_; }
    RenderQuality quality() const { return quality_; }

    // Ping-pong buffers. Stages read `front()` and write `back()`, then swap().
    WorkingImage& front() { return front_; }
    WorkingImage& back() { return back_; }
    void swap() { front_.swap(back_); }

    RenderStats& stats() { return stats_; }

private:
    const ParamSnapshot& params_;
    TimeInfo time_;
    RenderQuality quality_;
    WorkingImage front_;
    WorkingImage back_;
    RenderStats stats_;
};

}  // namespace pd
