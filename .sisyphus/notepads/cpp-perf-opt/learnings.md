# Learnings: C++ Performance Optimizations

## T1-1: RDTSC → std::chrono replacement

- The `read_tsc()` function used x86-specific `__rdtsc()` / inline asm, making it non-portable.
- The conversion `(cycles_elapsed * 10) / 30` hardcoded a 3GHz assumption. On any other frequency, timing is WRONG.
- `std::chrono::high_resolution_clock` is portable, monotonic, and already used in `collect_parallel()` — mirroring that pattern was straightforward.
- `#include <intrin.h>` was only needed for `__rdtsc()` on MSVC — removed safely.

## T1-2: Manual software prefetch removal

- `PREFETCH(&(*it))` called `_mm_prefetch` / `__builtin_prefetch` on the next element.
- Modern CPUs (since ~2010) have hardware prefetchers that outperform manual prefetch hints for simple sequential access patterns.
- Manual prefetch in a tight loop can actually DEGRADE performance by polluting cache with data not yet needed.
- The `if (it != end) [[likely]]` guard existed solely to protect the prefetch — removed with it.

## T1-3: alignas(64) → std::hardware_destructive_interference_size

- `std::hardware_destructive_interference_size` is the C++17 portable way to avoid false sharing on cache lines.
- Defined in `<new>` header.
- On x86-64, this resolves to 64 on most compilers, but is future-proof for architectures with different cache line sizes (e.g., Apple M1 = 128).
- MSVC 2022+ compiled this without issues under C++23.
- clangd LSP reported "no member" because it wasn't configured with the project's C++23 flags — the actual build was fine.
- `alignas(64)` was previously used on all 5 atomic fields in `PipelineStats`.

## Build verification

- Configured with `-DDPB_BUILD_TESTS=OFF -DDPB_BUILD_BENCHMARKS=OFF` to avoid Catch2/benchmark network fetch.
- `cmake --build build --config Release` compiled all 3 examples (`basic_example`, `async_example`, `parallel_example`) cleanly.
- Only warning was pre-existing MSVC `/Ob2` override — unrelated to these changes.

## T1-4: Lock-Free Work-Stealing Thread Pool

### Design
- Replaced the mutex+condition_variable+shared-queue design with a **Chase-Lev work-stealing deque** per worker.
- Each worker owns a 4096-slot ring buffer (`std::function<void()> ring[RING_SIZE]`), a `bottom` atomic (owner-only writes, relaxed), and a `top` atomic (CAS by stealers, acquire-release).
- **No mutex or condition_variable in the hot path.** The only synchronization is atomic CAS on `top` during steal, and a `seq_cst` fence in `pop()` for StoreLoad ordering.

### Chase-Lev Algorithm
- `push()`: Owner stores task at `ring[bottom & MASK]`, then `bottom.store(release)` to publish.
- `pop()`: Owner decrements `bottom`, issues a `seq_cst` fence (StoreLoad barrier), reads `top` with `acquire`. If `nb > t`, returns the task directly (no contention). If `nb == t`, races with stealers via CAS on `top`.
- `steal()`: Reads `top` (acquire) and `bottom` (acquire). If `t < b`, CAS on `top` to claim the task. CAS failure means another stealer (or `pop()`) won the race.

### Shutdown Safety
- `workers_` (unique_ptr<Worker>) declared BEFORE `threads_` (jthread) → reverse destruction order destroys `threads_` first, `workers_` second.
- This guarantees worker data (ring buffers, atomics) outlives running threads. No use-after-free during jthread auto-join.
- `~ThreadPool() = default` — jthreads auto-call `request_stop()` then `join()` in order. Workers check `st.stop_requested()` before each steal attempt.

### `std::jthread` Integration
- Each worker lambda `[this, i](std::stop_token st)` receives the jthread's own stop token.
- Cooperative interruption: workers check `st.stop_requested()` at the top of each loop iteration and before each steal attempt.
- No manual `std::atomic<bool> stop_` flag needed.

### Enqueue Distribution
- Round-robin via `next_enqueue_.fetch_add(1, relaxed) % n`.
- If all worker rings are full (4096 tasks each), the enqueuer spin-yields and retries. In practice, this never happens for pipeline workloads.

### Idle Backoff
- Workers that find no work (own deque empty, all steals failed) spin 32 times with `yield()`, then sleep for 100µs.
- This avoids busy-waiting while keeping latency low for bursty workloads.

### CAS `const` Issue
- `compare_exchange_strong(expected, desired, ...)` requires `expected` to be a non-const lvalue reference (it mutates on failure).
- Using `const std::size_t` for the expected value causes a compile error on MSVC: "cannot convert argument 1 from 'const size_t' to '_Ty &'".
- Fixed by using non-const locals for CAS arguments. In `pop()`, used a separate `cas_t` variable to preserve the original `t` for the `bottom.store(t)` fallback path.

### Build Verification
- `cmake --build build --config Release` compiled all 3 examples (`async_example`, `basic_example`, `parallel_example`) cleanly.
- Only warning: pre-existing MSVC `/Ob2` override — unrelated.
- clangd false-positively reports "No member named 'jthread' in namespace 'std'" due to missing C++23 compile flags in clangd config — same issue as T1-3.
