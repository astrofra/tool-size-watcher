#include "filesystem.h"

#if defined(__APPLE__)
#include <dirent.h>
#include <errno.h>
#include <pwd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#elif defined(_WIN32)
#include "windows_utf8.h"

#include <shlobj.h>
#else
#error "Tool Size Watcher currently supports macOS and Windows"
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <set>
#include <sstream>

namespace tsw {
namespace {

#if defined(_WIN32)
const char kPathSeparator = '\\';
#else
const char kPathSeparator = '/';
#endif

bool IsPathSeparator(char value) {
#if defined(_WIN32)
    return value == '\\' || value == '/';
#else
    return value == '/';
#endif
}

bool CharactersEqualForPath(char left, char right) {
#if defined(_WIN32)
    if (IsPathSeparator(left) && IsPathSeparator(right)) {
        return true;
    }
    return std::tolower(static_cast<unsigned char>(left)) ==
           std::tolower(static_cast<unsigned char>(right));
#else
    return left == right;
#endif
}

bool StartsWith(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool PathStartsWith(const std::string& value, const std::string& prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }
    for (std::size_t index = 0; index < prefix.size(); ++index) {
        if (!CharactersEqualForPath(value[index], prefix[index])) {
            return false;
        }
    }
    return true;
}

bool IsRootPath(const std::string& path) {
#if defined(_WIN32)
    return path.size() == 3 && std::isalpha(static_cast<unsigned char>(path[0])) && path[1] == ':' &&
           IsPathSeparator(path[2]);
#else
    return path == "/";
#endif
}

std::string TrimTrailingSlashes(const std::string& path) {
    if (path.empty()) {
        return path;
    }

    std::string trimmed = path;
    while (!IsRootPath(trimmed) && !trimmed.empty() && IsPathSeparator(trimmed[trimmed.size() - 1])) {
        trimmed.erase(trimmed.size() - 1);
    }
    return trimmed;
}

bool PathEqualsOrIsInside(const std::string& path, const std::string& root) {
    const std::string trimmed_path = TrimTrailingSlashes(path);
    const std::string trimmed_root = TrimTrailingSlashes(root);
    if (trimmed_root.empty() || !PathStartsWith(trimmed_path, trimmed_root)) {
        return false;
    }
    if (trimmed_path.size() == trimmed_root.size()) {
        return true;
    }
    if (IsRootPath(trimmed_root)) {
        return true;
    }
    return IsPathSeparator(trimmed_path[trimmed_root.size()]);
}

std::string JoinPath(const std::string& left, const std::string& right) {
    if (left.empty()) {
        return right;
    }
    if (IsPathSeparator(left[left.size() - 1])) {
        return left + right;
    }
    return left + kPathSeparator + right;
}

std::string LowercaseAscii(const std::string& value) {
    std::string lower = value;
    for (std::size_t index = 0; index < lower.size(); ++index) {
        lower[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[index])));
    }
    return lower;
}

std::string::size_type FindLastPathSeparator(const std::string& path) {
    for (std::string::size_type index = path.size(); index > 0; --index) {
        if (IsPathSeparator(path[index - 1])) {
            return index - 1;
        }
    }
    return std::string::npos;
}

#if defined(__APPLE__)

std::string GetHomeDirectory() {
    const char* home_env = std::getenv("HOME");
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
    return !IsHiddenSystemVolume(mount_path);
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

#elif defined(_WIN32)

std::string KnownFolderPath(REFKNOWNFOLDERID folder_id) {
    PWSTR wide_path = NULL;
    if (SHGetKnownFolderPath(folder_id, KF_FLAG_DEFAULT, NULL, &wide_path) != S_OK || wide_path == NULL) {
        return std::string();
    }
    const std::string path = windows::WideToUtf8(wide_path);
    CoTaskMemFree(wide_path);
    return TrimTrailingSlashes(path);
}

std::vector<std::string> BuildProtectedRoots() {
    std::vector<std::string> roots;
    const KNOWNFOLDERID* const folder_ids[] = {
        &FOLDERID_Desktop,
        &FOLDERID_Documents,
        &FOLDERID_Downloads
    };
    for (std::size_t index = 0; index < sizeof(folder_ids) / sizeof(folder_ids[0]); ++index) {
        const std::string path = KnownFolderPath(*folder_ids[index]);
        if (!path.empty()) {
            roots.push_back(path);
        }
    }
    return roots;
}

bool IsSupportedDriveType(UINT drive_type) {
    return drive_type == DRIVE_REMOVABLE || drive_type == DRIVE_FIXED || drive_type == DRIVE_REMOTE ||
           drive_type == DRIVE_CDROM || drive_type == DRIVE_RAMDISK;
}

std::string WindowsErrorMessage(DWORD error_code) {
    LPWSTR buffer = NULL;
    const DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                                            FORMAT_MESSAGE_IGNORE_INSERTS,
                                        NULL, error_code, 0, reinterpret_cast<LPWSTR>(&buffer), 0, NULL);
    if (length == 0 || buffer == NULL) {
        return "Windows error " + std::to_string(static_cast<unsigned long long>(error_code));
    }
    std::wstring message(buffer, length);
    LocalFree(buffer);
    while (!message.empty() && (message[message.size() - 1] == L'\r' || message[message.size() - 1] == L'\n' ||
                                message[message.size() - 1] == L' ')) {
        message.erase(message.size() - 1);
    }
    return windows::WideToUtf8(message);
}

EntryType DetectEntryType(const WIN32_FIND_DATAW& data) {
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        return EntryType::Symlink;
    }
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return EntryType::Directory;
    }
    return EntryType::File;
}

uint64_t LogicalFileSize(const WIN32_FIND_DATAW& data) {
    return (static_cast<uint64_t>(data.nFileSizeHigh) << 32U) | static_cast<uint64_t>(data.nFileSizeLow);
}

uint64_t AllocatedFileSize(const std::string& path, const WIN32_FIND_DATAW& data) {
    if ((data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        // Do not dereference symlinks or other reparse points just to measure their targets.
        return 0;
    }

    const std::wstring wide_path = windows::ToExtendedLengthPath(path);
    DWORD high = 0;
    SetLastError(ERROR_SUCCESS);
    const DWORD low = GetCompressedFileSizeW(wide_path.c_str(), &high);
    if (low == INVALID_FILE_SIZE && GetLastError() != ERROR_SUCCESS) {
        return LogicalFileSize(data);
    }
    return (static_cast<uint64_t>(high) << 32U) | static_cast<uint64_t>(low);
}

std::wstring DirectorySearchPattern(const std::string& path) {
    std::wstring pattern = windows::ToExtendedLengthPath(path);
    if (!pattern.empty() && pattern[pattern.size() - 1] != L'\\' && pattern[pattern.size() - 1] != L'/') {
        pattern.push_back(L'\\');
    }
    pattern.push_back(L'*');
    return pattern;
}

#endif

const std::vector<std::string>& ProtectedRoots() {
    static const std::vector<std::string> roots = BuildProtectedRoots();
    return roots;
}

struct ScanAccumulator {
    uint64_t total_bytes = 0;
    bool denied = false;
    bool error = false;
    bool cancelled = false;
};

#if defined(__APPLE__)
void MarkTraversalFailure(ScanAccumulator* accumulator, int error_code) {
    if (error_code == EACCES || error_code == EPERM) {
        accumulator->denied = true;
    } else {
        accumulator->error = true;
    }
}
#elif defined(_WIN32)
void MarkTraversalFailure(ScanAccumulator* accumulator, DWORD error_code) {
    if (error_code == ERROR_ACCESS_DENIED || error_code == ERROR_PRIVILEGE_NOT_HELD) {
        accumulator->denied = true;
    } else {
        accumulator->error = true;
    }
}
#endif

bool ShouldCancel(const std::function<bool()>& should_cancel) {
    return should_cancel && should_cancel();
}

void ReportProgress(const std::function<void(uint64_t)>& on_progress, const ScanAccumulator& accumulator) {
    if (on_progress) {
        on_progress(accumulator.total_bytes);
    }
}

#if defined(__APPLE__)
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
        if (name != "." && name != "..") {
            const std::string full_path = JoinPath(path, name);
            struct stat status;
            if (lstat(full_path.c_str(), &status) != 0) {
                MarkTraversalFailure(accumulator, errno);
            } else if (S_ISDIR(status.st_mode)) {
                if (status.st_dev == root_device &&
                    !ShouldSkipProtectedDirectory(scan_root, full_path, options)) {
                    ScanDirectoryRecursive(full_path, scan_root, options, root_device, should_cancel, on_progress,
                                           accumulator);
                }
            } else {
                // lstat measures the link entry itself and never follows a symlinked directory.
                accumulator->total_bytes += ToAllocatedBytes(status);
                ReportProgress(on_progress, *accumulator);
            }
        }

        if (accumulator->cancelled) {
            break;
        }
        errno = 0;
        entry = readdir(directory);
    }

    if (!accumulator->cancelled && errno != 0) {
        MarkTraversalFailure(accumulator, errno);
    }
    closedir(directory);
}
#elif defined(_WIN32)
void ScanDirectoryRecursive(const std::string& path,
                            const std::string& scan_root,
                            const ScanOptions& options,
                            const std::function<bool()>& should_cancel,
                            const std::function<void(uint64_t)>& on_progress,
                            ScanAccumulator* accumulator) {
    if (ShouldCancel(should_cancel)) {
        accumulator->cancelled = true;
        return;
    }

    WIN32_FIND_DATAW data;
    const std::wstring pattern = DirectorySearchPattern(path);
    HANDLE search = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, NULL,
                                     FIND_FIRST_EX_LARGE_FETCH);
    if (search == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER) {
        search = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, NULL, 0);
    }
    if (search == INVALID_HANDLE_VALUE) {
        MarkTraversalFailure(accumulator, GetLastError());
        return;
    }

    for (;;) {
        if (ShouldCancel(should_cancel)) {
            accumulator->cancelled = true;
            break;
        }

        const std::wstring wide_name(data.cFileName);
        if (wide_name != L"." && wide_name != L"..") {
            const std::string full_path = JoinPath(path, windows::WideToUtf8(wide_name));
            const EntryType type = DetectEntryType(data);
            if (type == EntryType::Directory) {
                if (!ShouldSkipProtectedDirectory(scan_root, full_path, options)) {
                    ScanDirectoryRecursive(full_path, scan_root, options, should_cancel, on_progress, accumulator);
                }
            } else {
                // Reparse points are classified as symlinks and contribute zero bytes, so their targets are never followed.
                accumulator->total_bytes += AllocatedFileSize(full_path, data);
                ReportProgress(on_progress, *accumulator);
            }
        }

        if (accumulator->cancelled) {
            break;
        }
        if (!FindNextFileW(search, &data)) {
            const DWORD error_code = GetLastError();
            if (error_code != ERROR_NO_MORE_FILES) {
                MarkTraversalFailure(accumulator, error_code);
            }
            break;
        }
    }
    FindClose(search);
}
#endif

}  // namespace

std::vector<VolumeInfo> EnumerateVolumes() {
    std::vector<VolumeInfo> volumes;

#if defined(__APPLE__)
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
#elif defined(_WIN32)
    const DWORD required = GetLogicalDriveStringsW(0, NULL);
    if (required == 0) {
        return volumes;
    }

    std::vector<wchar_t> drive_buffer(static_cast<std::size_t>(required) + 1U, L'\0');
    if (GetLogicalDriveStringsW(required, &drive_buffer[0]) == 0) {
        return volumes;
    }

    const wchar_t* drive = &drive_buffer[0];
    while (*drive != L'\0') {
        const std::wstring root(drive);
        const UINT drive_type = GetDriveTypeW(root.c_str());
        if (IsSupportedDriveType(drive_type)) {
            wchar_t label[MAX_PATH + 1] = {0};
            wchar_t fs_name[MAX_PATH + 1] = {0};
            GetVolumeInformationW(root.c_str(), label, MAX_PATH + 1, NULL, NULL, NULL, fs_name, MAX_PATH + 1);

            ULARGE_INTEGER available;
            ULARGE_INTEGER total;
            ULARGE_INTEGER free_total;
            available.QuadPart = 0;
            total.QuadPart = 0;
            free_total.QuadPart = 0;
            GetDiskFreeSpaceExW(root.c_str(), &available, &total, &free_total);

            VolumeInfo volume;
            volume.mount_path = windows::WideToUtf8(root);
            volume.fs_type = windows::WideToUtf8(fs_name);
            volume.total_bytes = total.QuadPart;
            volume.free_bytes = available.QuadPart;
            volume.is_network = drive_type == DRIVE_REMOTE;

            const std::string drive_name = volume.mount_path.substr(0, 2);
            const std::string volume_label = windows::WideToUtf8(label);
            volume.name = volume_label.empty() ? drive_name : volume_label + " (" + drive_name + ")";
            volumes.push_back(volume);
        }
        drive += root.size() + 1U;
    }
#endif

    std::sort(volumes.begin(), volumes.end(), [](const VolumeInfo& left, const VolumeInfo& right) {
#if defined(__APPLE__)
        if (left.mount_path == "/") {
            return true;
        }
        if (right.mount_path == "/") {
            return false;
        }
#endif
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

#if defined(__APPLE__)
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
        if (name != "." && name != "..") {
            const std::string full_path = JoinPath(path, name);
            struct stat status;
            if (lstat(full_path.c_str(), &status) == 0) {
                EntryInfo info;
                info.name = name;
                info.full_path = full_path;
                info.type = DetectEntryType(status);
                info.size_bytes = info.type == EntryType::Directory ? 0 : ToAllocatedBytes(status);
                info.status = info.type == EntryType::Directory ? ScanStatus::NotScanned : ScanStatus::Ready;
                listing.entries.push_back(info);
            }
        }
        errno = 0;
        entry = readdir(directory);
    }
    if (errno != 0) {
        listing.error_message = std::strerror(errno);
    }
    closedir(directory);
#elif defined(_WIN32)
    WIN32_FIND_DATAW data;
    const std::wstring pattern = DirectorySearchPattern(path);
    HANDLE search = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, NULL,
                                     FIND_FIRST_EX_LARGE_FETCH);
    if (search == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER) {
        search = FindFirstFileExW(pattern.c_str(), FindExInfoBasic, &data, FindExSearchNameMatch, NULL, 0);
    }
    if (search == INVALID_HANDLE_VALUE) {
        const DWORD error_code = GetLastError();
        if (error_code == ERROR_FILE_NOT_FOUND) {
            listing.success = true;
        } else {
            listing.error_message = WindowsErrorMessage(error_code);
        }
        return listing;
    }

    listing.success = true;
    for (;;) {
        const std::wstring wide_name(data.cFileName);
        if (wide_name != L"." && wide_name != L"..") {
            EntryInfo info;
            info.name = windows::WideToUtf8(wide_name);
            info.full_path = JoinPath(path, info.name);
            info.type = DetectEntryType(data);
            info.size_bytes = info.type == EntryType::Directory ? 0 : AllocatedFileSize(info.full_path, data);
            info.status = info.type == EntryType::Directory ? ScanStatus::NotScanned : ScanStatus::Ready;
            listing.entries.push_back(info);
        }
        if (!FindNextFileW(search, &data)) {
            const DWORD error_code = GetLastError();
            if (error_code != ERROR_NO_MORE_FILES) {
                listing.error_message = WindowsErrorMessage(error_code);
            }
            break;
        }
    }
    FindClose(search);
#endif

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

#if defined(__APPLE__)
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
#elif defined(_WIN32)
    const std::wstring wide_path = windows::ToExtendedLengthPath(path);
    const DWORD attributes = GetFileAttributesW(wide_path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        ScanSummary summary;
        const DWORD error_code = GetLastError();
        summary.status = (error_code == ERROR_ACCESS_DENIED || error_code == ERROR_PRIVILEGE_NOT_HELD)
                             ? ScanStatus::Denied
                             : ScanStatus::Error;
        return summary;
    }
    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        ScanSummary summary;
        summary.status = ScanStatus::Error;
        return summary;
    }
    ScanDirectoryRecursive(path, path, options, should_cancel, on_progress, &accumulator);
#endif

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
    if (options.include_protected_paths || !IsProtectedPath(candidate_path)) {
        return false;
    }
    return !IsProtectedPath(scan_root);
}

std::string BaseName(const std::string& path) {
    const std::string trimmed = TrimTrailingSlashes(path);
    if (trimmed.empty() || IsRootPath(trimmed)) {
        return trimmed;
    }
    const std::string::size_type slash = FindLastPathSeparator(trimmed);
    if (slash == std::string::npos) {
        return trimmed;
    }
    return trimmed.substr(slash + 1);
}

std::string ParentPath(const std::string& path) {
    const std::string trimmed = TrimTrailingSlashes(path);
    if (trimmed.empty() || IsRootPath(trimmed)) {
        return trimmed;
    }
    const std::string::size_type slash = FindLastPathSeparator(trimmed);
    if (slash == std::string::npos) {
        return trimmed;
    }
#if defined(_WIN32)
    if (slash == 2 && trimmed.size() >= 2 && trimmed[1] == ':') {
        return trimmed.substr(0, 3);
    }
#else
    if (slash == 0) {
        return "/";
    }
#endif
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
