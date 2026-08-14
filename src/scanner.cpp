#include "scanner.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
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
    workers_.reserve(worker_count);
    for (std::size_t index = 0; index < worker_count; ++index) {
        workers_.push_back(std::thread(&ScanScheduler::WorkerLoop, this));
    }
}

ScanScheduler::~ScanScheduler() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stop_ = true;
    }
    condition_.notify_all();

    for (std::size_t index = 0; index < workers_.size(); ++index) {
        workers_[index].join();
    }
}

void ScanScheduler::SetActiveEpoch(uint64_t epoch) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        active_epoch_.store(epoch);
        tasks_.clear();
        pending_tokens_.clear();
    }
    condition_.notify_all();
}

bool ScanScheduler::Enqueue(const std::string& path, uint64_t epoch) {
    const std::string token = MakeTaskToken(path, epoch);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!pending_tokens_.insert(token).second) {
            return false;
        }

        ScanTask task;
        task.path = path;
        task.token = token;
        task.epoch = epoch;
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

void ScanScheduler::PushEvent(const ScanEvent& event) {
    std::lock_guard<std::mutex> lock(events_mutex_);
    events_.push_back(event);
}

void ScanScheduler::WorkerLoop() {
    for (;;) {
        ScanTask task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this]() { return stop_ || !tasks_.empty(); });
            if (stop_ && tasks_.empty()) {
                return;
            }

            task = tasks_.front();
            tasks_.pop_front();
        }

        if (task.epoch != active_epoch_.load()) {
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
            [this, &task]() { return task.epoch != active_epoch_.load(); },
            [this, &task, &last_reported_bytes, &last_report_time](uint64_t bytes) {
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

        if (task.epoch != active_epoch_.load()) {
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
