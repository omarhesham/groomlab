// SPDX-License-Identifier: Apache-2.0
#include "groomlab/ramp.h"

#include <algorithm>

namespace groomlab {

void Ramp::sort() {
    std::stable_sort(keys.begin(), keys.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
}

float Ramp::operator()(float t) const {
    if (keys.empty()) return 1.0f;
    if (!(t > keys.front().first)) return keys.front().second;  // also NaN
    if (t >= keys.back().first) return keys.back().second;
    const auto hi = std::upper_bound(keys.begin(), keys.end(), t, [](float x, const auto& k) { return x < k.first; });
    const auto lo = hi - 1;
    const float span = hi->first - lo->first;
    if (span <= 0.0f) return hi->second;
    const float f = (t - lo->first) / span;
    if (straight) return lo->second + (hi->second - lo->second) * f;

    // Monotone cubic Hermite: tangents from the neighboring secants.
    const std::size_t i = static_cast<std::size_t>(lo - keys.begin());
    auto secant = [&](std::size_t k) {  // slope of span k (keys k to k + 1)
        const float h = keys[k + 1].first - keys[k].first;
        return h > 0.0f ? (keys[k + 1].second - keys[k].second) / h : 0.0f;
    };
    auto tangent = [&](std::size_t k) {
        if (k == 0) return secant(0);
        if (k + 1 == keys.size()) return secant(k - 1);
        const float d0 = secant(k - 1), d1 = secant(k);
        if (d0 * d1 <= 0.0f) return 0.0f;  // a peak or valley: flat
        const float h0 = keys[k].first - keys[k - 1].first, h1 = keys[k + 1].first - keys[k].first;
        return 3.0f * (h0 + h1) / ((2.0f * h1 + h0) / d0 + (h1 + 2.0f * h0) / d1);
    };
    const float m0 = tangent(i) * span, m1 = tangent(i + 1) * span;
    const float f2 = f * f, f3 = f2 * f;
    return (2.0f * f3 - 3.0f * f2 + 1.0f) * lo->second + (f3 - 2.0f * f2 + f) * m0 +
           (-2.0f * f3 + 3.0f * f2) * hi->second + (f3 - f2) * m1;
}

}  // namespace groomlab
