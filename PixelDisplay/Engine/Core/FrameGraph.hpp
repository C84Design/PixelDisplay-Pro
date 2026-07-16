// PixelDisplay Pro — Engine/Core/FrameGraph.hpp
//
// The FrameGraph is the ordered list of stages produced from a ParamSnapshot.
// It is compiled once per distinct parameter configuration (cheap) and reused
// across frames and tiles. Disabled feature groups drop their stages, so the
// graph shrinks for cheap looks / draft mode.
//
// The graph owns its stages. Because stages are stateless, a compiled graph is
// immutable after build and safe to execute from many threads concurrently.
#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "Engine/Core/ParamSnapshot.hpp"
#include "Engine/Core/Stage.hpp"

namespace pd {

class FrameGraph {
public:
    /// Build the stage list for a snapshot. Empty when the effect is passthrough.
    static FrameGraph compile(const ParamSnapshot& params);

    /// Topology key for a snapshot, computed without allocating stages. Two
    /// snapshots with the same key share a compiled graph (see ResourceCache).
    static std::uint64_t topologyKey(const ParamSnapshot& params);

    const std::vector<std::unique_ptr<Stage>>& stages() const { return stages_; }
    bool empty() const { return stages_.empty(); }

    /// Topology hash: identifies which stages are present (enabled-feature
    /// bitset). Used as a cache key so identical configurations reuse a graph.
    std::uint64_t topologyHash() const { return topologyHash_; }

private:
    std::vector<std::unique_ptr<Stage>> stages_;
    std::uint64_t topologyHash_ = 0;
};

}  // namespace pd
