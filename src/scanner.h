#pragma once

#include <stdint.h>

#include <atomic>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>
#include <condition_variable>

#include "filesystem.h"

namespace tsw {

struct ScanEvent {
    enum class Kind {
        Started,
        Completed
    };

    Kind kind = Kind::Started;
    std::string path;
    uint64_t epoch = 0;
    ScanSummary summary;
};

class ScanScheduler {
public:
    explicit ScanScheduler(std::size_t worker_count = 2);
    ~ScanScheduler();

    void SetActiveEpoch(uint64_t epoch);
    bool Enqueue(const std::string& path, uint64_t epoch);
    std::vector<ScanEvent> DrainEvents();

private:
    struct ScanTask {
        std::string path;
        std::string token;
        uint64_t epoch = 0;
    };

    void WorkerLoop();

    std::deque<ScanTask> tasks_;
    std::vector<ScanEvent> events_;
    std::vector<std::thread> workers_;
    std::mutex mutex_;
    std::mutex events_mutex_;
    std::condition_variable condition_;
    std::set<std::string> pending_tokens_;
    std::atomic<uint64_t> active_epoch_;
    bool stop_ = false;
};

}  // namespace tsw
