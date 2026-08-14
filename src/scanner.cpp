#include "scanner.h"

#include <condition_variable>
#include <mutex>
#include <set>

namespace tsw {
namespace {

std::string MakeTaskToken(const std::string& path, uint64_t epoch) {
    return path + "#" + std::to_string(epoch);
}

}  // namespace

ScanScheduler::ScanScheduler(std::size_t worker_count) {
    if (worker_count == 0) {
        worker_count = 1;
    }

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

        {
            std::lock_guard<std::mutex> lock(events_mutex_);
            ScanEvent started;
            started.kind = ScanEvent::Kind::Started;
            started.path = task.path;
            started.epoch = task.epoch;
            events_.push_back(started);
        }

        ScanEvent completed;
        completed.kind = ScanEvent::Kind::Completed;
        completed.path = task.path;
        completed.epoch = task.epoch;
        completed.summary = ComputeDirectorySize(task.path);

        {
            std::lock_guard<std::mutex> lock(events_mutex_);
            events_.push_back(completed);
        }

        {
            std::lock_guard<std::mutex> lock(mutex_);
            pending_tokens_.erase(task.token);
        }
    }
}

}  // namespace tsw

