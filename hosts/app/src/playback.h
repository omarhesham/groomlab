// SPDX-License-Identifier: Apache-2.0
#pragma once

// The timeline's clock, without Qt: which frame playback is on at a moment, from the wall
// clock. Playback never waits for a frame to finish drawing: the frame shown at any instant is
// the one the clock says, so a slow groom skips frames rather than slowing down (as Maya's
// "play every frame" off), and a fast one shows each. The tests check the arithmetic alone.

#include <cstddef>

namespace groomlab::app {

struct PlaybackRange {
    double first = 1.0, last = 1.0;  // inclusive frames
    double fps = 24.0;               // frames per second of the clock
    bool loop = true;                // past the last frame: around to the first (else stop there)
};

class PlaybackClock {
public:
    // Starts at `frame` (clamped into the range) at wall time `seconds`.
    void start(const PlaybackRange& range, double frame, double seconds);
    void stop() { playing_ = false; }
    bool playing() const { return playing_; }
    // The frame to show at wall time `seconds`: whole frames from where it started, wrapped
    // (loop) or held at the last frame (no loop: `finished` says so). Before start: the start frame.
    double frameAt(double seconds, bool* finished = nullptr) const;
    const PlaybackRange& range() const { return range_; }

private:
    PlaybackRange range_;
    double startFrame_ = 1.0;
    double startSeconds_ = 0.0;
    bool playing_ = false;
};

}  // namespace groomlab::app
