#pragma once

#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

namespace tsw {

struct VolumeInfo {
    std::string name;
    std::string mount_path;
    std::string fs_type;
    uint64_t total_bytes = 0;
    uint64_t free_bytes = 0;
    bool is_network = false;
};

enum class EntryType {
    File,
    Directory,
    Symlink,
    Other
};

enum class ScanStatus {
    NotScanned,
    Queued,
    Scanning,
    Ready,
    Denied,
    Error
};

struct EntryInfo {
    std::string name;
    std::string full_path;
    EntryType type = EntryType::Other;
    uint64_t size_bytes = 0;
    ScanStatus status = ScanStatus::NotScanned;
};

struct DirectoryListing {
    bool success = false;
    std::string error_message;
    std::vector<EntryInfo> entries;
};

struct ScanSummary {
    uint64_t size_bytes = 0;
    ScanStatus status = ScanStatus::NotScanned;
};

std::vector<VolumeInfo> EnumerateVolumes();
DirectoryListing ListDirectory(const std::string& path);
ScanSummary ComputeDirectorySize(const std::string& path);
ScanSummary ComputeDirectorySize(const std::string& path, const std::function<bool()>& should_cancel);
ScanSummary ComputeDirectorySize(const std::string& path,
                                const std::function<bool()>& should_cancel,
                                const std::function<void(uint64_t)>& on_progress);

std::string BaseName(const std::string& path);
std::string ParentPath(const std::string& path);
std::string FormatBytes(uint64_t bytes);
std::string EntryTypeToString(EntryType type);
std::string ScanStatusToString(ScanStatus status);

}  // namespace tsw
