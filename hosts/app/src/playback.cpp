// SPDX-License-Identifier: Apache-2.0
#include "playback.h"

#include <algorithm>
#include <cmath>

namespace groomlab::app {

void PlaybackClock::start(const PlaybackRange& range, double frame, double seconds) {
    range_ = range;
    if (range_.last < range_.first) range_.last = range_.first;
    if (!(range_.fps > 0.0)) range_.fps = 24.0;
    startFrame_ = std::clamp(frame, range_.first, range_.last);
    startSeconds_ = seconds;
    playing_ = true;
}

double PlaybackClock::frameAt(double seconds, bool* finished) const {
    if (finished) *finished = false;
    const double elapsed = std::max(0.0, seconds - startSeconds_);
    const double advanced = std::floor(elapsed * range_.fps);
    const double span = range_.last - range_.first + 1.0;  // frames in the loop
    double frame = startFrame_ + advanced;
    if (frame > range_.last) {
        if (range_.loop) {
            frame = range_.first + std::fmod(frame - range_.first, span);
        } else {
            frame = range_.last;
            if (finished) *finished = true;
        }
    }
    return frame;
}

}  // namespace groomlab::app
