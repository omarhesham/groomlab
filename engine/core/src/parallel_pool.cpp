// SPDX-License-Identifier: Apache-2.0
// The parallel backend of the C API's library (groomlab.dll, libgroomlab.so): a pool of the
// library's own threads, so the library loads no TBB and never meets a host's (a process holds
// one tbb.dll; a host that loaded another would hand the library that one).
//
// A parallel call cuts its range into chunks (about 8 per thread, none smaller than half the
// grain, as TBB splits) that every thread claims from one counter: the caller runs chunks too,
// and while chunks it handed out run elsewhere it helps with any other call's (a nested loop),
// so a loop inside a parallel body never waits on a thread that waits on it. Idle threads spin a
// moment before they sleep: an evaluation makes its parallel calls in quick succession. A call
// wakes as many sleeping threads as it has chunks beyond its own (a 2-chunk loop one, not every
// sleeper to spin and sleep again: an evaluation issues hundreds of small loops).
//
// The threads are the cores'; GROOMLAB_THREADS (read when the pool is made) and
// setParallelThreads cap how many take part (the rest sleep until the cap rises), for a host
// that runs the library inside a thread arena of its own.

#include "groomlab/parallel.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdlib>
#include <exception>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <dlfcn.h>
#endif

namespace groomlab::detail {
namespace {

// One parallel call: its chunks, claimed by the threads that run them.
struct Job {
    RangeBody body = nullptr;
    const void* context = nullptr;
    std::size_t begin = 0, end = 0, chunk = 1, chunkCount = 0;
    std::atomic<std::size_t> next{0};      // the next chunk to claim
    std::atomic<std::size_t> finished{0};  // chunks run (or skipped after a failure)
    std::atomic<bool> failed{false};
    std::mutex mutex;                      // error, done
    std::condition_variable doneSignal;
    std::exception_ptr error;              // the first chunk's that threw
    bool done = false;
};

// Claims and runs one of the job's chunks; false when none was left.
bool runChunk(Job& job) {
    const std::size_t c = job.next.fetch_add(1, std::memory_order_relaxed);
    if (c >= job.chunkCount) return false;
    if (!job.failed.load(std::memory_order_relaxed)) {
        const std::size_t lo = job.begin + c * job.chunk;
        const std::size_t hi = std::min(job.end, lo + job.chunk);
        try {
            job.body(job.context, lo, hi);
        } catch (...) {
            std::lock_guard<std::mutex> lock(job.mutex);
            if (!job.error) job.error = std::current_exception();
            job.failed.store(true, std::memory_order_relaxed);
        }
    }
    // acq_rel: the last chunk's thread sees every other chunk's writes, and hands them on.
    if (job.finished.fetch_add(1, std::memory_order_acq_rel) + 1 == job.chunkCount) {
        std::lock_guard<std::mutex> lock(job.mutex);
        job.done = true;
        job.doneSignal.notify_all();
    }
    return true;
}

bool exhausted(const Job& job) { return job.next.load(std::memory_order_relaxed) >= job.chunkCount; }

// Spins (pausing the core) until `ready` or about `micros` have passed; whether it got ready.
template <class Ready>
bool spinFor(int micros, const Ready& ready) {
    const auto until = std::chrono::steady_clock::now() + std::chrono::microseconds(micros);
    for (unsigned i = 1;; ++i) {
        if (ready()) return true;
        if ((i & 63u) == 0 && std::chrono::steady_clock::now() >= until) return false;
        std::this_thread::yield();
    }
}

constexpr int kSpinMicros = 100;

// The cap GROOMLAB_THREADS asks for (0: none, or not a number).
std::size_t threadsFromEnvironment() {
    const char* text = std::getenv("GROOMLAB_THREADS");
    if (!text || !*text) return 0;
    char* end = nullptr;
    const unsigned long n = std::strtoul(text, &end, 10);
    return end && *end == '\0' ? static_cast<std::size_t>(n) : 0;
}

class Pool {
public:
    Pool() {
        cores_ = std::max(1u, std::thread::hardware_concurrency());
        setLimit(threadsFromEnvironment());
        if (cores_ < 2) return;
#ifdef _WIN32
        // The workers run the library's code until the process ends: a host's FreeLibrary must
        // not unmap it under them. (libgroomlab.so links with -z nodelete for the same reason.)
        HMODULE self = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
                           reinterpret_cast<LPCWSTR>(&parallelRange), &self);
#elif defined(__APPLE__)
        // (macOS's linker has no -z nodelete: the image holding this code is opened once more,
        // RTLD_NODELETE, and that reference is never closed: it stays even if a host dlcloses its own.)
        static const char anchor = 0;
        Dl_info image{};
        if (dladdr(&anchor, &image) && image.dli_fname) dlopen(image.dli_fname, RTLD_LAZY | RTLD_NODELETE);
#endif
        for (std::size_t i = 1; i < cores_; ++i) std::thread([this, i] { work(i); }).detach();
    }

    // The threads taking part: the caller and the workers below the cap.
    std::size_t threads() const { return limit_.load(std::memory_order_acquire); }

    // Caps the threads at `threads` (0, or past the cores: every core). Workers past the cap sleep
    // until it rises again.
    void setLimit(std::size_t threads) {
        const std::size_t limit = threads == 0 ? cores_ : std::min(threads, cores_);
        {
            std::lock_guard<std::mutex> lock(mutex_);
            limit_.store(limit, std::memory_order_release);
        }
        limitSignal_.notify_all();  // workers the cap lets back in
        wake_.notify_all();         // sleepers the cap now leaves out: they go and wait on it
    }

    void post(const std::shared_ptr<Job>& job) {
        // Sleepers woken: one per chunk beyond the caller's (the others would spin and sleep
        // again); workers awake pick the job up as they look.
        int wake = 0;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            open_.push_back(job);
            openCount_.store(open_.size(), std::memory_order_release);
            wake = static_cast<int>(std::min<std::size_t>(job->chunkCount - 1, static_cast<std::size_t>(sleepers_)));
        }
        for (int i = 0; i < wake; ++i) wake_.notify_one();
    }

    // Drops a job whose chunks are all claimed.
    void retire(const Job* job) {
        std::lock_guard<std::mutex> lock(mutex_);
        open_.erase(std::remove_if(open_.begin(), open_.end(), [&](const std::shared_ptr<Job>& j) { return j.get() == job; }),
                    open_.end());
        openCount_.store(open_.size(), std::memory_order_release);
    }

    // The newest job with chunks left (a nested loop before the loop it runs in), or null.
    std::shared_ptr<Job> pick() {
        if (openCount_.load(std::memory_order_acquire) == 0) return nullptr;
        std::lock_guard<std::mutex> lock(mutex_);
        while (!open_.empty()) {
            if (!exhausted(*open_.back())) return open_.back();
            open_.pop_back();
        }
        openCount_.store(0, std::memory_order_release);
        return nullptr;
    }

private:
    // Worker `index` (1 and up; 0 is whoever calls): past the cap it sleeps until the cap rises.
    void work(std::size_t index) {
        for (;;) {
            if (index >= limit_.load(std::memory_order_acquire)) {
                std::unique_lock<std::mutex> lock(mutex_);
                limitSignal_.wait(lock, [&] { return index < limit_.load(std::memory_order_acquire); });
                continue;
            }
            if (std::shared_ptr<Job> job = pick()) {
                while (runChunk(*job)) {
                }
                continue;
            }
            if (spinFor(kSpinMicros, [&] { return openCount_.load(std::memory_order_acquire) != 0; })) continue;
            std::unique_lock<std::mutex> lock(mutex_);
            ++sleepers_;
            wake_.wait(lock, [&] { return !open_.empty() || index >= limit_.load(std::memory_order_acquire); });
            --sleepers_;
        }
    }

    std::size_t cores_ = 1;
    std::atomic<std::size_t> limit_{1};
    std::mutex mutex_;
    std::condition_variable wake_;
    std::condition_variable limitSignal_;
    std::vector<std::shared_ptr<Job>> open_;  // jobs that may have chunks left, oldest first
    std::atomic<std::size_t> openCount_{0};
    int sleepers_ = 0;  // workers waiting on wake_
};

// Made at the first parallel call and never destroyed: its threads live until the process ends.
Pool& pool() {
    static Pool* const instance = new Pool();
    return *instance;
}

}  // namespace

void parallelRange(std::size_t begin, std::size_t end, std::size_t grain, RangeBody body, const void* context) {
    if (end <= begin) return;
    Pool& p = pool();
    const std::size_t count = end - begin;
    const std::size_t smallest = std::max<std::size_t>(1, (grain + 1) / 2);
    const std::size_t pieces = std::min(p.threads() * 8, count / smallest);
    if (pieces < 2) {
        body(context, begin, end);
        return;
    }
    auto job = std::make_shared<Job>();
    job->body = body;
    job->context = context;
    job->begin = begin;
    job->end = end;
    job->chunk = (count + pieces - 1) / pieces;
    job->chunkCount = (count + job->chunk - 1) / job->chunk;
    p.post(job);
    while (runChunk(*job)) {
    }
    p.retire(job.get());

    // Chunks still running elsewhere: help with other calls' (nested loops) meanwhile, then sleep.
    const auto finished = [&] { return job->finished.load(std::memory_order_acquire) == job->chunkCount; };
    while (!finished()) {
        if (std::shared_ptr<Job> other = p.pick()) {
            runChunk(*other);
            continue;
        }
        if (spinFor(kSpinMicros, [&] { return finished() || p.pick() != nullptr; })) continue;
        std::unique_lock<std::mutex> lock(job->mutex);
        job->doneSignal.wait(lock, [&] { return job->done; });
        break;
    }
    if (job->failed.load(std::memory_order_relaxed)) {
        std::lock_guard<std::mutex> lock(job->mutex);
        std::rethrow_exception(job->error);
    }
}

}  // namespace groomlab::detail

namespace groomlab {

void setParallelThreads(std::size_t threads) { detail::pool().setLimit(threads); }

std::size_t parallelThreads() { return detail::pool().threads(); }

// (The pool is the library's own, shared by every caller: fn runs as it is.)
void runWithThreads(std::size_t, void (*fn)(void* context), void* context) { fn(context); }

}  // namespace groomlab
