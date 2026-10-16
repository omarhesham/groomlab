// SPDX-License-Identifier: Apache-2.0
#pragma once

// A GroomSession evaluated off the caller's thread (docs/SESSION.md, "Evaluating off the UI
// thread"). A host whose UI must not wait for an evaluation (Maya: the commit of a change the GPU
// look preview already draws, a drag's or a brush stroke's release) starts it here and takes the
// result when it is ready, drawing what it has meanwhile.
//
// A session is single-threaded: from start() until wait() returns it belongs to the worker, and the
// host must not call it (nor read its last()); one evaluation runs at a time, in the order started.
// The host decides what its own queries answer meanwhile (Maya: the frame from before the commit,
// and anything that reads the session waits for it: groom_shape.h).
//
//   AsyncEvaluation async(session);
//   async.start(request);              // the session is the worker's
//   ... draw, answer the UI ...
//   if (async.ready()) {               // or wait() whenever the session is needed: it blocks
//       const ViewResult* view = async.wait();   // the session is the host's again
//   }

#include <atomic>
#include <cstdint>
#include <exception>
#include <functional>
#include <memory>
#include <thread>

#include "groomlab/groom_session.h"

namespace groomlab {

class AsyncEvaluation {
public:
    enum class State {
        Idle,     // nothing started, or the last result taken: the session is the host's
        Running,  // the worker evaluates: the session is the worker's
        Done,     // evaluated, not yet taken (wait() returns at once)
    };
    // Runs on the worker before the evaluation: setters whose work (parsing a text) should not
    // hold up the host. Given the session.
    using Prepare = std::function<void(GroomSession&)>;

    explicit AsyncEvaluation(GroomSession& session) : session_(session) {}
    // Waits for an evaluation still running (its result dropped, anything it threw swallowed).
    ~AsyncEvaluation();
    AsyncEvaluation(const AsyncEvaluation&) = delete;
    AsyncEvaluation& operator=(const AsyncEvaluation&) = delete;

    // Evaluates `request` (after `prepare`) on a worker thread. One started before and not taken
    // is waited for first and its result dropped (what it threw too): evaluations chain, each on
    // the session as the one before left it, so a later one reuses its work.
    void start(const ViewRequest& request, Prepare prepare = {});
    State state() const { return state_.load(std::memory_order_acquire); }
    bool pending() const { return state() != State::Idle; }  // started and not taken
    bool ready() const { return state() == State::Done; }    // wait() will not block
    // Waits for the evaluation started (if any) and hands the session back: its result (the
    // session's last(), valid until the session evaluates again), or null when nothing was
    // started. What escaped the worker (thrown by `prepare`, out of memory) is rethrown here, once;
    // the session is the host's again either way.
    const ViewResult* wait();
    // The last evaluation's time on the worker, `prepare` included (ms), and how many ran.
    double lastMilliseconds() const { return lastMs_; }
    std::uint64_t evaluations() const { return evaluations_; }
    // How many threads the worker's evaluation takes (parallel.h runWithThreads; 0: every core, the
    // default), from the next start(): fewer leave cores to the host, whose UI thread draws (with
    // TBB loops of its own, Maya's) while the worker evaluates.
    void setThreads(std::size_t threads) { threads_ = threads; }
    std::size_t threads() const { return threads_; }

private:
    GroomSession& session_;
    std::size_t threads_ = 0;
    std::thread worker_;
    std::atomic<State> state_{State::Idle};
    const ViewResult* result_ = nullptr;
    std::exception_ptr error_;
    double lastMs_ = 0.0;
    std::uint64_t evaluations_ = 0;
};

// Detached evaluations (GroomSession::detach) run in the order started, each on a worker thread
// that first waits for the one before: start() never waits (a brush stroke's release while the
// last stroke's commit still evaluates: the new one goes on from it), and the session stays the
// host's throughout. wait() waits for every one started and hands back the last.
//
//   DetachedRunner runner;
//   runner.start(session.detach(request, runner.last()));  // after the one before, if any
//   ... the session goes on (a stroke's dabs) ...
//   if (runner.ready()) session.adopt(*runner.wait()), session.evaluate(request);
class DetachedRunner {
public:
    DetachedRunner() = default;
    ~DetachedRunner();  // waits for them all
    DetachedRunner(const DetachedRunner&) = delete;
    DetachedRunner& operator=(const DetachedRunner&) = delete;

    // Runs `run` once the one started before has run. Throws (nothing started) when no thread
    // can be made: the ones before are waited for first.
    void start(std::shared_ptr<DetachedEvaluation> run);
    // The last started and not taken (null: none), for a chained detach.
    const std::shared_ptr<DetachedEvaluation>& last() const { return last_; }
    bool pending() const { return last_ != nullptr; }
    bool ready() const { return last_ && last_->ran(); }  // wait() will not block
    // Waits for every one started; the last (its result), null when none is pending.
    std::shared_ptr<DetachedEvaluation> wait();
    // Threads each run's loops take (parallel.h runWithThreads; 0: every core), from the next start.
    void setThreads(std::size_t threads) { threads_ = threads; }
    std::uint64_t started() const { return started_; }

private:
    std::size_t threads_ = 0;
    std::thread worker_;  // the last one's (it joins the one before)
    std::shared_ptr<DetachedEvaluation> last_;
    std::uint64_t started_ = 0;
};

}  // namespace groomlab
