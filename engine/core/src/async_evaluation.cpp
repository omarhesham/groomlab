// SPDX-License-Identifier: Apache-2.0
#include "groomlab/async_evaluation.h"

#include <chrono>
#include <utility>

#include "groomlab/parallel.h"

namespace groomlab {

AsyncEvaluation::~AsyncEvaluation() {
    try {
        wait();
    } catch (...) {
    }
}

void AsyncEvaluation::start(const ViewRequest& request, Prepare prepare) {
    if (pending()) {
        try {
            wait();  // in order: the session as the one before left it
        } catch (...) {
        }
    }
    result_ = nullptr;
    error_ = nullptr;
    state_.store(State::Running, std::memory_order_release);
    try {
        worker_ = std::thread([this, request, prepare = std::move(prepare), threads = threads_] {
            const auto started = std::chrono::steady_clock::now();
            struct Work {
                AsyncEvaluation* self;
                const ViewRequest* request;
                const Prepare* prepare;
            } work{this, &request, &prepare};
            runWithThreads(threads, [](void* context) {
                const Work& w = *static_cast<const Work*>(context);
                try {
                    if (*w.prepare) (*w.prepare)(w.self->session_);
                    w.self->result_ = &w.self->session_.evaluate(*w.request);
                } catch (...) {
                    w.self->error_ = std::current_exception();
                }
            }, &work);
            lastMs_ = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
            state_.store(State::Done, std::memory_order_release);
        });
    } catch (...) {
        state_.store(State::Idle, std::memory_order_release);  // no thread: nothing started
        throw;
    }
}

const ViewResult* AsyncEvaluation::wait() {
    if (!pending()) return nullptr;
    if (worker_.joinable()) worker_.join();
    state_.store(State::Idle, std::memory_order_release);
    ++evaluations_;
    if (error_) {
        std::exception_ptr error = std::exchange(error_, nullptr);
        result_ = nullptr;
        std::rethrow_exception(error);
    }
    return std::exchange(result_, nullptr);
}

DetachedRunner::~DetachedRunner() {
    if (worker_.joinable()) worker_.join();
}

void DetachedRunner::start(std::shared_ptr<DetachedEvaluation> run) {
    auto before = std::make_shared<std::thread>(std::move(worker_));
    try {
        worker_ = std::thread([before, run, threads = threads_] {
            if (before->joinable()) before->join();  // in order: this one may go on from it
            runWithThreads(threads, [](void* r) { static_cast<DetachedEvaluation*>(r)->run(); }, run.get());
        });
    } catch (...) {
        if (before->joinable()) before->join();
        throw;
    }
    last_ = std::move(run);
    ++started_;
}

std::shared_ptr<DetachedEvaluation> DetachedRunner::wait() {
    if (worker_.joinable()) worker_.join();
    return std::exchange(last_, nullptr);
}

}  // namespace groomlab
