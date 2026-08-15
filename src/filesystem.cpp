#include "filesystem.h"

#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <set>
#include <sstream>

namespace tsw {
namespace {

bool StartsWith(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool PathEqualsOrIsInside(const std::string& path, const std::string& root) {
    if (root.empty()) {
        return false;
    }
    if (path == root) {
        return true;
    }
    return path.size() > root.size() && path.compare(0, root.size(), root) == 0 && path[root.size()] == '/';
}

std::string TrimTrailingSlashes(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    std::string trimmed = path;
    while (trimmed.size() > 1 && trimmed[trimmed.size() - 1] == '/') {
        trimmed.erase(trimmed.size() - 1);
    }
    return trimmed;
}

std::string JoinPath(const std::string& left, const std::string& right) {
    if (left.empty() || left == "/") {
        return "/" + right;
    }
    return TrimTrailingSlashes(left) + "/" + right;
}

std::string LowercaseAscii(const std::string& value) {
    std::string lower = value;
    for (std::size_t index = 0; index < lower.size(); ++index) {
        lower[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[index])));
    }
    return lower;
}

std::string GetHomeDirectory() {
    const char* home_env = getenv("HOME");
    if (home_env != NULL && home_env[0] != '\0') {
        return TrimTrailingSlashes(home_env);
    }

    const struct passwd* password_entry = getpwuid(getuid());
    if (password_entry != NULL && password_entry->pw_dir != NULL && password_entry->pw_dir[0] != '\0') {
        return TrimTrailingSlashes(password_entry->pw_dir);
    }

    return std::string();
}

std::vector<std::string> BuildProtectedRoots() {
    const std::string home = GetHomeDirectory();
    if (home.empty()) {
        return std::vector<std::string>();
    }

    std::vector<std::string> roots;
    roots.push_back(home + "/Desktop");
    roots.push_back(home + "/Documents");
    roots.push_back(home + "/Downloads");
    roots.push_back(home + "/Pictures/Photos Library.photoslibrary");
    roots.push_back(home + "/Library/Application Support/AddressBook");
    roots.push_back(home + "/Library/Calendars");
    roots.push_back(home + "/Library/Group Containers/group.com.apple.calendar");
    roots.push_back(home + "/Library/Group Containers/group.com.apple.reminders");
    roots.push_back(home + "/Library/Mail");
    roots.push_back(home + "/Library/Messages");
    roots.push_back(home + "/Library/Reminders");
    roots.push_back(home + "/Library/Safari");
    roots.push_back(home + "/Library/HomeKit");
    roots.push_back(home + "/Library/IdentityServices");
    return roots;
}

const std::vector<std::string>& ProtectedRoots() {
    static const std::vector<std::string> roots = BuildProtectedRoots();
    return roots;
}

bool IsHiddenSystemVolume(const std::string& mount_path) {
    static const char* const kHiddenPrefixes[] = {
        "/Volumes/com.apple.TimeMachine.localsnapshots",
        "/System/Volumes/Preboot",
        "/System/Volumes/Update"
    };

    for (std::size_t index = 0; index < sizeof(kHiddenPrefixes) / sizeof(kHiddenPrefixes[0]); ++index) {
        if (StartsWith(mount_path, kHiddenPrefixes[index])) {
            return true;
        }
    }
    return false;
}

bool IsUserVisibleMount(const struct statfs& entry) {
    const std::string mount_path(entry.f_mntonname);
    if (mount_path == "/") {
        return true;
    }

    if (!StartsWith(mount_path, "/Volumes/")) {
        return false;
    }

    if ((entry.f_flags & MNT_DONTBROWSE) != 0) {
        return false;
    }

    if (IsHiddenSystemVolume(mount_path)) {
        return false;
    }

    return true;
}

bool IsNetworkMount(const struct statfs& entry) {
    return (entry.f_flags & MNT_LOCAL) == 0;
}

uint64_t ToAllocatedBytes(const struct stat& status) {
    if (status.st_blocks > 0) {
        return static_cast<uint64_t>(status.st_blocks) * 512ULL;
    }
    return static_cast<uint64_t>(status.st_size);
}

EntryType DetectEntryType(const struct stat& status) {
    if (S_ISDIR(status.st_mode)) {
        return EntryType::Directory;
    }
    if (S_ISREG(status.st_mode)) {
        return EntryType::File;
    }
    if (S_ISLNK(status.st_mode)) {
        return EntryType::Symlink;
    }
    return EntryType::Other;
}

struct ScanAccumulator {
    uint64_t total_bytes = 0;
    bool denied = false;
    bool error = false;
    bool cancelled = false;
};

void MarkTraversalFailure(ScanAccumulator* accumulator, int error_code) {
    if (error_code == EACCES || error_code == EPERM) {
        accumulator->denied = true;
        return;
    }
    accumulator->error = true;
}

bool ShouldCancel(const std::function<bool()>& should_cancel) {
    return should_cancel && should_cancel();
}

void ReportProgress(const std::function<void(uint64_t)>& on_progress, const ScanAccumulator& accumulator) {
    if (on_progress) {
        on_progress(accumulator.total_bytes);
    }
}

void ScanDirectoryRecursive(const std::string& path,
                            const std::string& scan_root,
                            const ScanOptions& options,
                            dev_t root_device,
                            const std::function<bool()>& should_cancel,
                            const std::function<void(uint64_t)>& on_progress,
                            ScanAccumulator* accumulator) {
    if (ShouldCancel(should_cancel)) {
        accumulator->cancelled = true;
        return;
    }

    DIR* directory = opendir(path.c_str());
    if (directory == NULL) {
        MarkTraversalFailure(accumulator, errno);
        return;
    }

    errno = 0;
    struct dirent* entry = readdir(directory);
    while (entry != NULL) {
        if (ShouldCancel(should_cancel)) {
            accumulator->cancelled = true;
            break;
        }

        const std::string name(entry->d_name);
        if (name == "." || name == "..") {
            entry = readdir(directory);
            continue;
        }

        const std::string full_path = JoinPath(path, name);
        struct stat status;
        if (lstat(full_path.c_str(), &status) != 0) {
            MarkTraversalFailure(accumulator, errno);
            entry = readdir(directory);
            continue;
        }

        if (S_ISDIR(status.st_mode)) {
            if (status.st_dev == root_device &&
                !ShouldSkipProtectedDirectory(scan_root, full_path, options)) {
                ScanDirectoryRecursive(full_path, scan_root, options, root_device, should_cancel, on_progress,
                                       accumulator);
            }
        } else {
            accumulator->total_bytes += ToAllocatedBytes(status);
            ReportProgress(on_progress, *accumulator);
        }

        if (accumulator->cancelled) {
            break;
        }

        entry = readdir(directory);
    }

    if (!accumulator->cancelled && errno != 0) {
        MarkTraversalFailure(accumulator, errno);
    }

    closedir(directory);
}

}  // namespace

std::vector<VolumeInfo> EnumerateVolumes() {
    std::vector<VolumeInfo> volumes;

    struct statfs* mount_points = NULL;
    const int count = getmntinfo(&mount_points, MNT_NOWAIT);
    if (count <= 0 || mount_points == NULL) {
        return volumes;
    }

    std::set<std::string> seen_mount_paths;
    for (int index = 0; index < count; ++index) {
        const struct statfs& entry = mount_points[index];
        if (!IsUserVisibleMount(entry)) {
            continue;
        }

        const std::string mount_path(entry.f_mntonname);
        if (!seen_mount_paths.insert(mount_path).second) {
            continue;
        }

        VolumeInfo volume;
        volume.mount_path = mount_path;
        volume.fs_type = entry.f_fstypename;
        volume.total_bytes = static_cast<uint64_t>(entry.f_blocks) * static_cast<uint64_t>(entry.f_bsize);
        volume.free_bytes = static_cast<uint64_t>(entry.f_bavail) * static_cast<uint64_t>(entry.f_bsize);
        volume.is_network = IsNetworkMount(entry);
        volume.name = BaseName(mount_path);
        volumes.push_back(volume);
    }

    std::sort(volumes.begin(), volumes.end(), [](const VolumeInfo& left, const VolumeInfo& right) {
        if (left.mount_path == "/") {
            return true;
        }
        if (right.mount_path == "/") {
            return false;
        }
        const std::string left_name = LowercaseAscii(left.name);
        const std::string right_name = LowercaseAscii(right.name);
        if (left_name != right_name) {
            return left_name < right_name;
        }
        return left.mount_path < right.mount_path;
    });

    return volumes;
}

DirectoryListing ListDirectory(const std::string& path) {
    DirectoryListing listing;
    DIR* directory = opendir(path.c_str());
    if (directory == NULL) {
        listing.error_message = std::strerror(errno);
        return listing;
    }

    listing.success = true;
    errno = 0;

    struct dirent* entry = readdir(directory);
    while (entry != NULL) {
        const std::string name(entry->d_name);
        if (name == "." || name == "..") {
            entry = readdir(directory);
            continue;
        }

        const std::string full_path = JoinPath(path, name);
        struct stat status;
        if (lstat(full_path.c_str(), &status) != 0) {
            entry = readdir(directory);
            continue;
        }

        EntryInfo info;
        info.name = name;
        info.full_path = full_path;
        info.type = DetectEntryType(status);
        info.size_bytes = (info.type == EntryType::Directory) ? 0 : ToAllocatedBytes(status);
        info.status = (info.type == EntryType::Directory) ? ScanStatus::NotScanned : ScanStatus::Ready;
        listing.entries.push_back(info);

        entry = readdir(directory);
    }

    if (errno != 0) {
        listing.error_message = std::strerror(errno);
    }

    closedir(directory);
    return listing;
}

ScanSummary ComputeDirectorySize(const std::string& path) {
    return ComputeDirectorySize(path, ScanOptions(), std::function<bool()>(), std::function<void(uint64_t)>());
}

ScanSummary ComputeDirectorySize(const std::string& path, const ScanOptions& options) {
    return ComputeDirectorySize(path, options, std::function<bool()>(), std::function<void(uint64_t)>());
}

ScanSummary ComputeDirectorySize(const std::string& path, const std::function<bool()>& should_cancel) {
    return ComputeDirectorySize(path, ScanOptions(), should_cancel, std::function<void(uint64_t)>());
}

ScanSummary ComputeDirectorySize(const std::string& path,
                                 const ScanOptions& options,
                                 const std::function<bool()>& should_cancel) {
    return ComputeDirectorySize(path, options, should_cancel, std::function<void(uint64_t)>());
}

ScanSummary ComputeDirectorySize(const std::string& path,
                                const std::function<bool()>& should_cancel,
                                const std::function<void(uint64_t)>& on_progress) {
    return ComputeDirectorySize(path, ScanOptions(), should_cancel, on_progress);
}

ScanSummary ComputeDirectorySize(const std::string& path,
                                const ScanOptions& options,
                                const std::function<bool()>& should_cancel,
                                const std::function<void(uint64_t)>& on_progress) {
    ScanAccumulator accumulator;
    struct stat root_status;
    if (lstat(path.c_str(), &root_status) != 0) {
        ScanSummary summary;
        summary.status = (errno == EACCES || errno == EPERM) ? ScanStatus::Denied : ScanStatus::Error;
        return summary;
    }

    if (!S_ISDIR(root_status.st_mode)) {
        ScanSummary summary;
        summary.status = ScanStatus::Error;
        return summary;
    }

    ScanDirectoryRecursive(path, path, options, root_status.st_dev, should_cancel, on_progress, &accumulator);

    ScanSummary summary;
    summary.size_bytes = accumulator.total_bytes;
    if (accumulator.cancelled) {
        summary.status = ScanStatus::NotScanned;
    } else if (accumulator.denied) {
        summary.status = ScanStatus::Denied;
    } else if (accumulator.error) {
        summary.status = ScanStatus::Error;
    } else {
        summary.status = ScanStatus::Ready;
    }
    return summary;
}

bool IsProtectedPath(const std::string& path) {
    const std::string trimmed = TrimTrailingSlashes(path);
    const std::vector<std::string>& roots = ProtectedRoots();
    for (std::size_t index = 0; index < roots.size(); ++index) {
        if (PathEqualsOrIsInside(trimmed, roots[index])) {
            return true;
        }
    }
    return false;
}

bool ShouldSkipProtectedDirectory(const std::string& scan_root,
                                  const std::string& candidate_path,
                                  const ScanOptions& options) {
    if (options.include_protected_paths) {
        return false;
    }
    if (!IsProtectedPath(candidate_path)) {
        return false;
    }
    return !IsProtectedPath(scan_root);
}

std::string BaseName(const std::string& path) {
    const std::string trimmed = TrimTrailingSlashes(path);
    if (trimmed.empty() || trimmed == "/") {
        return "/";
    }

    const std::string::size_type slash = trimmed.find_last_of('/');
    if (slash == std::string::npos) {
        return trimmed;
    }
    return trimmed.substr(slash + 1);
}

std::string ParentPath(const std::string& path) {
    const std::string trimmed = TrimTrailingSlashes(path);
    if (trimmed.empty() || trimmed == "/") {
        return "/";
    }

    const std::string::size_type slash = trimmed.find_last_of('/');
    if (slash == std::string::npos || slash == 0) {
        return "/";
    }
    return trimmed.substr(0, slash);
}

std::string FormatBytes(uint64_t bytes) {
    static const char* const kUnits[] = {"B", "KB", "MB", "GB", "TB", "PB"};
    double value = static_cast<double>(bytes);
    std::size_t unit_index = 0;
    while (value >= 1024.0 && unit_index + 1 < sizeof(kUnits) / sizeof(kUnits[0])) {
        value /= 1024.0;
        ++unit_index;
    }

    std::ostringstream stream;
    if (unit_index == 0) {
        stream << bytes << ' ' << kUnits[unit_index];
    } else {
        stream << std::fixed << std::setprecision(1) << value << ' ' << kUnits[unit_index];
    }
    return stream.str();
}

std::string EntryTypeToString(EntryType type) {
    switch (type) {
    case EntryType::File:
        return "File";
    case EntryType::Directory:
        return "Folder";
    case EntryType::Symlink:
        return "Symlink";
    case EntryType::Other:
    default:
        return "Other";
    }
}

std::string ScanStatusToString(ScanStatus status) {
    switch (status) {
    case ScanStatus::NotScanned:
        return "Not Scanned";
    case ScanStatus::Queued:
        return "Queued";
    case ScanStatus::Scanning:
        return "Scanning";
    case ScanStatus::Ready:
        return "Ready";
    case ScanStatus::Excluded:
        return "Excluded";
    case ScanStatus::Denied:
        return "Denied";
    case ScanStatus::Error:
    default:
        return "Error";
    }
}

}  // namespace tsw
