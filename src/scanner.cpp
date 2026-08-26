#include "scanner.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#if defined(__APPLE__)
#include <pthread.h>
#elif defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif
#include <set>

namespace tsw {
namespace {

std::string MakeTaskToken(const std::string& path, uint64_t epoch) {
    return path + "#" + std::to_string(epoch);
}

const uint64_t kProgressByteStep = 64ULL * 1024ULL * 1024ULL;
const std::chrono::milliseconds kProgressTimeStep(250);

}  // namespace

ScanScheduler::ScanScheduler(std::size_t worker_count) {
    if (worker_count == 0) {
        worker_count = 1;
    }

    active_epoch_.store(0);
    stop_.store(false);
    workers_.reserve(worker_count);
    for (std::size_t index = 0; index < worker_count; ++index) {
        workers_.push_back(std::thread(&ScanScheduler::WorkerLoop, this));
    }
}

ScanScheduler::~ScanScheduler() {
    RequestStop();

    for (std::size_t index = 0; index < workers_.size(); ++index) {
        if (workers_[index].joinable()) {
            workers_[index].join();
        }
    }
}

void ScanScheduler::RequestStop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_.store(true);
        active_epoch_.fetch_add(1);
        tasks_.clear();
        pending_tokens_.clear();
    }
    condition_.notify_all();
}

void ScanScheduler::SetActiveEpoch(uint64_t epoch) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_.load()) {
            return;
        }
        active_epoch_.store(epoch);
        tasks_.clear();
        pending_tokens_.clear();
    }
    condition_.notify_all();
}

bool ScanScheduler::Enqueue(const std::string& path, uint64_t epoch) {
    return Enqueue(path, epoch, ScanOptions());
}

bool ScanScheduler::Enqueue(const std::string& path, uint64_t epoch, const ScanOptions& options) {
    const std::string token = MakeTaskToken(path, epoch);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (stop_.load()) {
            return false;
        }
        if (!pending_tokens_.insert(token).second) {
            return false;
        }

        ScanTask task;
        task.path = path;
        task.token = token;
        task.epoch = epoch;
        task.options = options;
        tasks_.push_back(task);
    }

    condition_.notify_one();
    return true;
}

std::vector<ScanEvent> ScanScheduler::DrainEvents() {
    std::lock_guard<std::mutex> lock(events_mutex_);
    std::vector<ScanEvent> drained;
    drained.swap(events_);
    return drained;
}

std::size_t ScanScheduler::WorkerCount() const {
    return workers_.size();
}

void ScanScheduler::PushEvent(const ScanEvent& event) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    events_.push_back(event);
}

void ScanScheduler::WorkerLoop() {
    // Keep background scanning from competing too aggressively with the file manager and UI responsiveness.
#if defined(__APPLE__)
    pthread_set_qos_class_self_np(QOS_CLASS_UTILITY, 0);
#elif defined(_WIN32)
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
#endif

    for (;;) {
        ScanTask task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this]() { return stop_.load() || !tasks_.empty(); });
            if (stop_.load() && tasks_.empty()) {
                return;
            }

            task = tasks_.front();
            tasks_.pop_front();
        }

        if (stop_.load() || task.epoch != active_epoch_.load()) {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_tokens_.erase(task.token);
            continue;
        }

        ScanEvent started;
        started.kind = ScanEvent::Kind::Started;
        started.path = task.path;
        started.epoch = task.epoch;
        PushEvent(started);

        uint64_t last_reported_bytes = 0;
        std::chrono::steady_clock::time_point last_report_time = std::chrono::steady_clock::now();

        ScanEvent completed;
        completed.kind = ScanEvent::Kind::Completed;
        completed.path = task.path;
        completed.epoch = task.epoch;
        completed.summary = ComputeDirectorySize(
            task.path,
            task.options,
            [this, &task]() { return stop_.load() || task.epoch != active_epoch_.load(); },
            [this, &task, &last_reported_bytes, &last_report_time](uint64_t bytes) {
                if (stop_.load()) {
                    return;
                }
                const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
                if (bytes > last_reported_bytes &&
                    (bytes - last_reported_bytes >= kProgressByteStep || now - last_report_time >= kProgressTimeStep)) {
                    last_reported_bytes = bytes;
                    last_report_time = now;

                    ScanEvent progress;
                    progress.kind = ScanEvent::Kind::Progress;
                    progress.path = task.path;
                    progress.epoch = task.epoch;
                    progress.summary.size_bytes = bytes;
                    progress.summary.status = ScanStatus::Scanning;
                    PushEvent(progress);
                }
            });

        if (stop_.load() || task.epoch != active_epoch_.load()) {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_tokens_.erase(task.token);
            continue;
        }

        PushEvent(completed);

        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_tokens_.erase(task.token);
        }
    }
}

}  // namespace tsw
