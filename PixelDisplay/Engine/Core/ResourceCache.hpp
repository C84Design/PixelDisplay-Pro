// PixelDisplay Pro — Engine/Core/ResourceCache.hpp
//
// Thread-safe memoization of expensive, parameter-derived resources. In M9 it
// caches compiled FrameGraphs by topology key; the same mechanism will cache GPU
// pipeline-state objects, procedural layout geometry, colour LUTs, and defect
// masks (DESIGN.md §9.3). Cached entries are immutable `shared_ptr<const T>`, so
// many render threads read them concurrently with no locking after build.
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "Engine/Core/FrameGraph.hpp"
#include "Engine/Core/ParamSnapshot.hpp"

namespace pd {

class ResourceCache {
public:
    /// Return the compiled graph for `params`, building and caching it on first
    /// use. The returned graph is immutable and safe to execute from many
    /// threads at once (stages are stateless).
    std::shared_ptr<const FrameGraph> graph(const ParamSnapshot& params) {
        const std::uint64_t key = FrameGraph::topologyKey(params);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = graphs_.find(key);
            if (it != graphs_.end()) return it->second;
        }
        // Build outside the fast path; double-check on insert to avoid races.
        auto built = std::make_shared<const FrameGraph>(FrameGraph::compile(params));
        std::lock_guard<std::mutex> lock(mutex_);
        auto [it, inserted] = graphs_.emplace(key, built);
        if (inserted) ++compileCount_;
        return it->second;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return graphs_.size();
    }

    /// Number of graphs actually compiled (cache misses). For diagnostics/tests.
    std::uint64_t compileCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return compileCount_;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        graphs_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::uint64_t, std::shared_ptr<const FrameGraph>> graphs_;
    std::uint64_t compileCount_ = 0;
};

}  // namespace pd
