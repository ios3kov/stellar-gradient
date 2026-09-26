#pragma once
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

namespace stellar {

// AE can render multiple frames concurrently. Track that pressure so a single
// frame does not consume the whole machine while MFR is already parallelizing.
inline std::atomic<unsigned> g_active_render_calls{0};

class RenderConcurrencyScope {
public:
    RenderConcurrencyScope() { g_active_render_calls.fetch_add(1, std::memory_order_relaxed); }
    ~RenderConcurrencyScope() { g_active_render_calls.fetch_sub(1, std::memory_order_relaxed); }
    RenderConcurrencyScope(const RenderConcurrencyScope&) = delete;
    RenderConcurrencyScope& operator=(const RenderConcurrencyScope&) = delete;
};

inline unsigned cpu_worker_count(int rows, int min_rows_per_worker = 96) {
    if (rows <= min_rows_per_worker) return 1;
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4;
    const unsigned active = std::max(1u, g_active_render_calls.load(std::memory_order_relaxed));
    const unsigned global_budget = std::min(hw, 8u);
    const unsigned per_render_budget = std::max(1u, global_budget / active);
    const unsigned by_work = static_cast<unsigned>((rows + min_rows_per_worker - 1) / min_rows_per_worker);
    return std::max(1u, std::min(per_render_budget, by_work));
}

// A tiny process-local worker pool. The previous implementation created and
// joined OS threads for every base/glow/diffusion pass and every large mip
// level. Reusing workers removes that overhead while preserving exactly the
// same row partitioning and floating-point math.
class ParallelPool {
public:
    static ParallelPool& instance() {
        static ParallelPool pool;
        return pool;
    }

    ParallelPool(const ParallelPool&) = delete;
    ParallelPool& operator=(const ParallelPool&) = delete;

    void enqueue(std::function<void()> task) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            queue_.push_back(std::move(task));
        }
        cv_.notify_one();
    }

private:
    ParallelPool() {
        unsigned hw = std::thread::hardware_concurrency();
        if (hw == 0) hw = 4;
        const unsigned count = std::max(1u, std::min(hw, 8u)) - 1u;
        workers_.reserve(count);
        for (unsigned i = 0; i < count; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    ~ParallelPool() {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            stopping_ = true;
        }
        cv_.notify_all();
        for (auto& worker : workers_) if (worker.joinable()) worker.join();
    }

    void worker_loop() {
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(mutex_);
                cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
                if (stopping_ && queue_.empty()) return;
                task = std::move(queue_.front());
                queue_.pop_front();
            }
            task();
        }
    }

    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<std::function<void()>> queue_;
    std::vector<std::thread> workers_;
    bool stopping_ = false;
};

struct ParallelLatch {
    explicit ParallelLatch(unsigned count) : remaining(count) {}
    void done() {
        if (remaining.fetch_sub(1, std::memory_order_acq_rel) == 1u) {
            std::lock_guard<std::mutex> lock(mutex);
            cv.notify_one();
        }
    }
    void wait() {
        if (remaining.load(std::memory_order_acquire) == 0u) return;
        std::unique_lock<std::mutex> lock(mutex);
        cv.wait(lock, [this] { return remaining.load(std::memory_order_acquire) == 0u; });
    }
    std::atomic<unsigned> remaining;
    std::mutex mutex;
    std::condition_variable cv;
};

template <class Fn>
inline void parallel_rows(int begin, int end, Fn fn, int min_rows_per_worker = 96) {
    const int rows = std::max(0, end - begin);
    const unsigned workers = cpu_worker_count(rows, min_rows_per_worker);
    if (workers <= 1) { fn(begin, end); return; }

    // Queue N-1 chunks and execute the final chunk on the calling AE render
    // thread. The latch guarantees Fn remains alive until all queued chunks end.
    const unsigned queued = workers - 1u;
    auto latch = std::make_shared<ParallelLatch>(queued);
    int start = begin;
    for (unsigned i = 0; i < workers; ++i) {
        const int remaining_rows = end - start;
        const int chunks = static_cast<int>(workers - i);
        const int count = (remaining_rows + chunks - 1) / chunks;
        const int stop = std::min(end, start + count);
        if (i + 1u == workers) {
            fn(start, stop);
        } else {
            Fn* fn_ptr = &fn;
            ParallelPool::instance().enqueue([fn_ptr, start, stop, latch] {
                (*fn_ptr)(start, stop);
                latch->done();
            });
        }
        start = stop;
    }
    latch->wait();
}

} // namespace stellar
