// SPDX-License-Identifier: Apache-2.0
// The core's parallel backend in a build without TBB: every loop in order, on the calling thread.

#include "groomlab/parallel.h"

namespace groomlab {

namespace detail {

void parallelRange(std::size_t begin, std::size_t end, std::size_t, RangeBody body, const void* context) {
    if (end > begin) body(context, begin, end);
}

}  // namespace detail

// One thread, whatever is asked (parallel.h).
void setParallelThreads(std::size_t) {}

std::size_t parallelThreads() { return 1; }

void runWithThreads(std::size_t, void (*fn)(void* context), void* context) { fn(context); }

}  // namespace groomlab
