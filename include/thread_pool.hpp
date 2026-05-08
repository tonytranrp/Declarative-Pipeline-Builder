#pragma once

#include <vector>
#include <thread>
#include <functional>
#include <future>
#include <atomic>
#include <cstddef>
#include <chrono>
#include <memory>

// Apple Clang's libc++ does not ship jthread/stop_token yet. Use <thread> + atomics.
#if defined(__cpp_lib_jthread) && __cpp_lib_jthread >= 201911L
#  if __has_include(<stop_token>)
#    include <stop_token>
#    define DPB_HAS_JTHREAD 1
#  endif
#endif
#ifndef DPB_HAS_JTHREAD
#  define DPB_HAS_JTHREAD 0
#endif

namespace dpb {

class ThreadPool {
    static constexpr std::size_t RING_SIZE = 4096;
    static constexpr std::size_t RING_MASK = RING_SIZE - 1;

    // Per-worker lock-free work-stealing deque (Chase-Lev single-producer, multi-consumer).
    // Only the owning worker pushes/pops at the bottom; other workers CAS-steal from the top.
    struct alignas(64) Worker {
        std::function<void()> ring[RING_SIZE];
        std::atomic<std::size_t> top{0};
        std::atomic<std::size_t> bottom{0};

        // Owner push (lock-free)
        [[nodiscard]] bool push(std::function<void()> f) noexcept {
            const std::size_t b = bottom.load(std::memory_order_relaxed);
            const std::size_t t = top.load(std::memory_order_acquire);
            if (b - t >= RING_SIZE) return false;          // full
            ring[b & RING_MASK] = std::move(f);
            bottom.store(b + 1, std::memory_order_release); // publish
            return true;
        }

        // Owner pop (lock-free)
        std::function<void()> pop() noexcept {
            const std::size_t b = bottom.load(std::memory_order_relaxed);
            if (b == 0) return {};
            const std::size_t nb = b - 1;
            bottom.store(nb, std::memory_order_relaxed);
            // StoreLoad barrier: ensure bottom update is visible before reading top
            std::atomic_thread_fence(std::memory_order_seq_cst);
            std::size_t t = top.load(std::memory_order_acquire);

            if (nb > t) {
                return std::move(ring[nb & RING_MASK]);
            }
            if (nb == t) {
                // Race for the last element with stealers
                std::size_t cas_t = t;
                if (top.compare_exchange_strong(cas_t, cas_t + 1,
                        std::memory_order_acq_rel, std::memory_order_relaxed)) {
                    bottom.store(nb + 1, std::memory_order_relaxed);
                    return std::move(ring[nb & RING_MASK]);
                }
                // CAS failed: stealer claimed it; cas_t reloaded with new top
                t = cas_t;
            }
            bottom.store(t, std::memory_order_relaxed);
            return {};
        }

        // Stealer CAS-pop from top (lock-free, multi-consumer)
        std::function<void()> steal() noexcept {
            std::size_t t = top.load(std::memory_order_acquire);
            const std::size_t b = bottom.load(std::memory_order_acquire);
            if (t >= b) return {};
            auto task = std::move(ring[t & RING_MASK]);
            if (!top.compare_exchange_strong(t, t + 1,
                    std::memory_order_acq_rel, std::memory_order_relaxed)) {
                return {}; // someone else stole it first
            }
            return task;
        }
    };

    // workers_ declared BEFORE threads_ → threads destroyed first (reverse order).
    // This guarantees worker data (ring buffers, atomics) outlives running threads.
    std::vector<std::unique_ptr<Worker>> workers_;
#if DPB_HAS_JTHREAD
    std::vector<std::jthread> threads_;
#else
    std::vector<std::thread> threads_;
    std::atomic<bool> stop_{false};
#endif
    std::atomic<std::size_t> next_enqueue_{0};

#if DPB_HAS_JTHREAD
    void worker_loop(std::size_t id, std::stop_token st) {
#else
    void worker_loop(std::size_t id) {
#endif
        auto& w = *workers_[id];
        const std::size_t n = workers_.size();
        int spins = 0;

#if DPB_HAS_JTHREAD
        while (!st.stop_requested()) {
#else
        while (!stop_.load(std::memory_order_acquire)) {
#endif
            // 1. Try own deque
            if (auto task = w.pop()) {
                task();
                spins = 0;
                continue;
            }

            // 2. Try stealing from others
            bool stolen = false;
#if DPB_HAS_JTHREAD
            for (std::size_t i = 1; i < n && !st.stop_requested(); ++i) {
#else
            for (std::size_t i = 1; i < n && !stop_.load(std::memory_order_acquire); ++i) {
#endif
                const std::size_t victim = (id + i) % n;
                if (auto task = workers_[victim]->steal()) {
                    task();
                    stolen = true;
                    break;
                }
            }
            if (stolen) {
                spins = 0;
                continue;
            }

            // 3. Backoff — brief spin then sleep
            if (spins < 32) {
                ++spins;
                std::this_thread::yield();
            } else {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }

public:
    // ThreadPool creates a fixed set of worker threads. The worker count is
    // independent of per-pipeline parallelism_ — see Pipeline::parallel() for
    // how chunk granularity vs pool workers interact.
    explicit ThreadPool(std::size_t num_threads = std::thread::hardware_concurrency()) {
        num_threads = (num_threads == 0) ? 1 : num_threads;
        for (std::size_t i = 0; i < num_threads; ++i) {
            workers_.push_back(std::make_unique<Worker>());
        }
        threads_.reserve(num_threads);
#if DPB_HAS_JTHREAD
        for (std::size_t i = 0; i < num_threads; ++i) {
            threads_.emplace_back([this, i](std::stop_token st) {
                worker_loop(i, st);
            });
        }
#else
        for (std::size_t i = 0; i < num_threads; ++i) {
            threads_.emplace_back([this, i]() {
                worker_loop(i);
            });
        }
#endif
    }

#if DPB_HAS_JTHREAD
    ~ThreadPool() = default;  // jthreads auto-stop & join; workers outlive threads
#else
    ~ThreadPool() {
        stop_.store(true, std::memory_order_release);
        for (auto& t : threads_) {
            if (t.joinable()) t.join();
        }
    }
#endif

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    // Enqueue a task — distributes round-robin across ALL workers.
    // Note: this uses workers_.size() (the pool's fixed worker count), NOT any
    // per-pipeline parallelism_ setting. The Pipeline::parallelism_ field controls
    // chunk granularity (how many tasks are submitted), while this method
    // distributes those tasks across all available workers. Work-stealing
    // automatically balances load when task count ≠ worker count.
    template<typename F, typename... Args>
    auto enqueue(F&& f, Args&&... args) -> std::future<decltype(f(args...))> {
        using return_type = decltype(f(args...));
        auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...));
        std::future<return_type> result = task->get_future();

        auto wrapper = [task]() { (*task)(); };
        const std::size_t n = workers_.size();
        const std::size_t start = next_enqueue_.fetch_add(1, std::memory_order_relaxed) % n;

        // Round-robin push; retry with yield if all workers are full
        for (;;) {
            for (std::size_t i = 0; i < n; ++i) {
                if (workers_[(start + i) % n]->push(wrapper)) {
                    return result;
                }
            }
            std::this_thread::yield();
        }
    }

    [[nodiscard]] std::size_t size() const noexcept { return workers_.size(); }

    template<typename Iter, typename Fn>
    void parallel_for(Iter begin, Iter end, Fn&& fn, std::size_t min_chunk_size = 1) {
        const auto count = static_cast<std::size_t>(end - begin);
        if (count == 0) return;

        const std::size_t num_workers = std::min(workers_.size(), count / min_chunk_size);
        if (num_workers <= 1) {
            for (Iter it = begin; it != end; ++it) fn(it);
            return;
        }

        const std::size_t chunk = count / num_workers;
        const std::size_t remainder = count % num_workers;
        std::vector<std::future<void>> futures;
        futures.reserve(num_workers);

        Iter chunk_begin = begin;
        for (std::size_t i = 0; i < num_workers; ++i) {
            const std::size_t chunk_size = chunk + (i < remainder ? 1 : 0);
            Iter chunk_end = chunk_begin + chunk_size;
            futures.push_back(enqueue([chunk_begin, chunk_end, &fn]() {
                for (Iter it = chunk_begin; it != chunk_end; ++it) fn(it);
            }));
            chunk_begin = chunk_end;
        }

        for (auto& f : futures) f.get();
    }
};

inline ThreadPool& global_thread_pool() {
    static ThreadPool pool;
    return pool;
}

} // namespace dpb
