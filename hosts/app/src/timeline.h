// SPDX-License-Identifier: Apache-2.0
#pragma once

// The floating timeline: one glass pill at the bottom of the viewport (above the brush bar in
// Groom mode) shown when the groom has animated inputs (a scalp or guide cache, a graph that
// reads the time). Transport buttons (to the start, play or pause, to the end), a scrubber (the
// playhead over the range, dragged or clicked), the frame in mono, the range's first and last
// frames, the fps, a Loop chip and a Follow chip (followScalp: the groom evaluated on the rest
// pose and carried by the scalp). With a Simulate node in the graph the pill grows its sim
// controls (Simulate: the range's frames simulated into the cache; Bake: written beside the
// groom for renders; Clear: the frames and bakes dropped) and the scrubber a strip under its
// track: the frames cached in memory in emerald, the baked ones in blue. Playback runs on the
// wall clock (playback.h): the window is asked to show the clock's frame whenever it moved, so a
// slow groom drops frames rather than slowing the clock (a live Simulate node steps the frames
// it is shown). The window evaluates and redraws; the timeline only says which frame.

#include <utility>
#include <vector>

#include <QElapsedTimer>
#include <QWidget>

#include "glass.h"
#include "playback.h"

class QLabel;
class QSpinBox;
class QTimer;

namespace groomlab::app {

// The scrubber: the range as a track with the playhead; a press or drag sets the frame.
class FrameScrubber : public QWidget {
    Q_OBJECT
public:
    explicit FrameScrubber(QWidget* parent = nullptr);
    void setRange(double first, double last);
    void setFrame(double frame);  // no signal
    double frame() const { return frame_; }
    // The simulation's frames: cached in memory and baked, runs of first..last (inclusive), drawn
    // as a strip under the track.
    using Runs = std::vector<std::pair<double, double>>;
    void setSimulatedFrames(Runs cached, Runs baked);
    const Runs& cachedFrames() const { return cached_; }
    const Runs& bakedFrames() const { return baked_; }
    QSize sizeHint() const override;

signals:
    void scrubbed(double frame);  // the user moved the playhead

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    void setFromX(qreal x);
    double first_ = 1.0, last_ = 1.0, frame_ = 1.0;
    bool dragging_ = false;
    Runs cached_, baked_;
};

class Timeline : public GlassPanel {
    Q_OBJECT
public:
    explicit Timeline(QWidget* parent = nullptr);
    // The range (frames, inclusive) and the fps; no signals.
    void setRange(double first, double last);
    void setFps(double fps);
    void setFrame(double frame);  // no signal; stays inside the range
    double frame() const { return frame_; }
    double first() const { return first_; }
    double last() const { return last_; }
    double fps() const { return fps_; }
    bool loop() const;
    // Playing: the clock starts at the current frame (or the first, when at the end without
    // loop) and the window hears frameChanged as it advances; stopping keeps the frame.
    void setPlaying(bool playing);
    bool playing() const { return clock_.playing(); }
    // The Follow chip: shown when the scalp has a rest pose to follow (`available`).
    void setFollowScalp(bool on, bool available);
    bool followScalp() const;
    // Frames the clock advanced past without being shown (the window counts what it showed).
    std::size_t droppedFrames() const { return dropped_; }
    // The sim controls (shown while the graph has a Simulate node) and the strip of the frames
    // simulated (FrameScrubber::setSimulatedFrames). Busy: Simulate or Bake is running, the
    // Simulate button reads Cancel (simulationCancelRequested) and the others wait.
    void setSimulationShown(bool shown);
    bool simulationShown() const;
    void setSimulatedFrames(FrameScrubber::Runs cached, FrameScrubber::Runs baked);
    const FrameScrubber* scrubber() const { return scrubber_; }
    void setSimulationBusy(bool busy);
    // The buttons (tests click them).
    RoundButton* simulateButton() const { return simulate_; }
    RoundButton* bakeButton() const { return bakeSim_; }
    RoundButton* clearButton() const { return clearSim_; }

signals:
    void frameChanged(double frame);  // scrubbed, stepped, or the clock moved
    void rangeChanged(double first, double last);
    void playingChanged(bool playing);
    void followScalpChanged(bool on);
    void simulateRequested();    // Simulate: the range's frames into the cache
    void bakeRequested();        // Bake: simulated and written beside the groom
    void invalidateRequested();  // Clear: the frames and bakes dropped
    void simulationCancelRequested();  // Cancel (the Simulate button while a run is busy)

private:
    void tick();  // the clock's frame, shown when it moved
    void refresh();
    double now() const { return wall_.nsecsElapsed() / 1e9; }

    double first_ = 1.0, last_ = 1.0, frame_ = 1.0, fps_ = 24.0;
    PlaybackClock clock_;
    QElapsedTimer wall_;
    QTimer* timer_ = nullptr;
    std::size_t dropped_ = 0;
    RoundButton* toStart_ = nullptr;
    RoundButton* play_ = nullptr;
    RoundButton* toEnd_ = nullptr;
    FrameScrubber* scrubber_ = nullptr;
    QLabel* frameLabel_ = nullptr;
    QSpinBox* firstBox_ = nullptr;
    QSpinBox* lastBox_ = nullptr;
    QLabel* fpsLabel_ = nullptr;
    RoundButton* loop_ = nullptr;
    RoundButton* follow_ = nullptr;
    QWidget* sim_ = nullptr;  // the sim controls
    RoundButton* simulate_ = nullptr;
    RoundButton* bakeSim_ = nullptr;
    RoundButton* clearSim_ = nullptr;
    bool updating_ = false;
    bool simBusy_ = false;
};

}  // namespace groomlab::app
