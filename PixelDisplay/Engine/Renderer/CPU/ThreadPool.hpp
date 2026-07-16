// PixelDisplay Pro — Engine/Renderer/CPU/ThreadPool.hpp
//
// A minimal work-stealing-free thread pool used by the CPU backend to run
// independent image tiles in parallel. Deliberately simple and dependency-free.
//
// Thread-safety: the pool itself is shared; parallelFor() blocks until all tiles
// complete, so no per-render state leaks between MFR worker threads. Each render
// call uses parallelFor with its own callable capturing only local state.
#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace pd::cpu {

class ThreadPool {
public:
    explicit ThreadPool(unsigned threads = 0) {
        if (threads == 0) {
            threads = std::thread::hardware_concurrency();
            if (threads == 0) threads = 1;
        }
        workers_.reserve(threads);
        for (unsigned i = 0; i < threads; ++i)
            workers_.emplace_back([this] { workerLoop(); });
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_) if (w.joinable()) w.join();
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    unsigned threadCount() const { return static_cast<unsigned>(workers_.size()); }

    /// Run fn(i) for i in [0, count) across the pool, blocking until all finish.
    /// Runs inline when count <= 1 to avoid scheduling overhead.
    ///
    /// Correctness note: we enqueue exactly one drain-task per worker (never one
    /// per item) and wait until every enqueued task has *returned* — not merely
    /// until the item counter empties. This guarantees no queued job outlives
    /// this stack frame, so the by-reference captures below can never dangle.
    void parallelFor(std::size_t count, const std::function<void(std::size_t)>& fn) {
        if (count == 0) return;
        if (count == 1) { fn(0); return; }

        std::atomic<std::size_t> next{0};
        const std::size_t taskCount = std::min<std::size_t>(count, workers_.size());

        // The completion counter is guarded by doneMutex (not atomic) and the
        // notify happens *under* that lock. This is deliberate: it guarantees no
        // worker touches doneMutex/doneCv after the waiter observes completion,
        // so these stack-local sync primitives can be safely destroyed when
        // parallelFor returns. (A lock-free counter + out-of-lock notify races
        // with the waiter's destruction on a spurious wakeup — verified via TSan.)
        std::size_t tasksCompleted = 0;
        std::mutex doneMutex;
        std::condition_variable doneCv;

        auto drain = [&] {
            for (;;) {
                std::size_t i = next.fetch_add(1, std::memory_order_relaxed);
                if (i >= count) break;
                fn(i);
            }
        };

        auto worker = [&] {
            drain();
            std::lock_guard<std::mutex> lock(doneMutex);
            if (++tasksCompleted == taskCount) doneCv.notify_one();
        };

        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (std::size_t i = 0; i < taskCount; ++i) jobs_.push(worker);
        }
        cv_.notify_all();

        // Participate on the calling thread too, then wait for the worker tasks.
        drain();
        std::unique_lock<std::mutex> lock(doneMutex);
        doneCv.wait(lock, [&] { return tasksCompleted == taskCount; });
    }

private:
    void workerLoop() {
        for (;;) {
            std::function<void()> job;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stop_ || !jobs_.empty(); });
                if (stop_ && jobs_.empty()) return;
                job = std::move(jobs_.front());
                jobs_.pop();
            }
            job();
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> jobs_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool stop_ = false;
};

}  // namespace pd::cpu
