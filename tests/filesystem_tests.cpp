#include "filesystem.h"

#if defined(_WIN32)
#include "windows_utf8.h"
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

bool Check(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << std::endl;
    }
    return condition;
}

struct TestDirectory {
    std::string path;
    std::string payload_path;

    bool Create() {
#if defined(_WIN32)
        wchar_t temp_path[MAX_PATH + 1] = {0};
        if (GetTempPathW(MAX_PATH + 1, temp_path) == 0) {
            return false;
        }

        wchar_t unique_path[MAX_PATH + 1] = {0};
        if (GetTempFileNameW(temp_path, L"tsw", 0, unique_path) == 0) {
            return false;
        }
        DeleteFileW(unique_path);
        if (!CreateDirectoryW(unique_path, NULL)) {
            return false;
        }

        path = tsw::windows::WideToUtf8(unique_path);
        const std::wstring wide_payload = std::wstring(unique_path) + L"\\size-\u00e9.bin";
        payload_path = tsw::windows::WideToUtf8(wide_payload);
        HANDLE file = CreateFileW(wide_payload.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE) {
            return false;
        }

        std::vector<char> bytes(8192, 'x');
        DWORD written = 0;
        const bool wrote = WriteFile(file, &bytes[0], static_cast<DWORD>(bytes.size()), &written, NULL) != FALSE;
        CloseHandle(file);
        return wrote && written == bytes.size();
#else
        char pattern[] = "/tmp/tool-size-watcher-tests-XXXXXX";
        char* created = mkdtemp(pattern);
        if (created == NULL) {
            return false;
        }
        path = created;
        payload_path = path + "/size-payload.bin";
        std::ofstream output(payload_path.c_str(), std::ios::binary);
        const std::string bytes(8192, 'x');
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        return output.good();
#endif
    }

    void Remove() {
        if (path.empty()) {
            return;
        }
#if defined(_WIN32)
        DeleteFileW(tsw::windows::Utf8ToWide(payload_path).c_str());
        RemoveDirectoryW(tsw::windows::Utf8ToWide(path).c_str());
#else
        unlink(payload_path.c_str());
        rmdir(path.c_str());
#endif
    }

    ~TestDirectory() {
        Remove();
    }
};

}  // namespace

int main() {
    bool passed = true;
    passed &= Check(tsw::FormatBytes(512) == "512 B", "byte formatting");
    passed &= Check(tsw::FormatBytes(1536) == "1.5 KB", "kilobyte formatting");

#if defined(_WIN32)
    passed &= Check(tsw::BaseName("C:\\Users\\Example") == "Example", "Windows basename");
    passed &= Check(tsw::ParentPath("C:\\Users") == "C:\\", "Windows parent path");
    passed &= Check(tsw::ParentPath("C:\\") == "C:\\", "Windows root parent");
#else
    passed &= Check(tsw::BaseName("/Users/example") == "example", "macOS basename");
    passed &= Check(tsw::ParentPath("/Users") == "/", "macOS parent path");
    passed &= Check(tsw::ParentPath("/") == "/", "macOS root parent");
#endif

    const std::vector<tsw::VolumeInfo> volumes = tsw::EnumerateVolumes();
    passed &= Check(!volumes.empty(), "volume enumeration");

    TestDirectory directory;
    passed &= Check(directory.Create(), "temporary directory creation");
    if (!directory.path.empty()) {
        const tsw::DirectoryListing listing = tsw::ListDirectory(directory.path);
        passed &= Check(listing.success, "temporary directory listing");
        passed &= Check(listing.entries.size() == 1, "temporary directory entry count");
        if (!listing.entries.empty()) {
            passed &= Check(listing.entries[0].type == tsw::EntryType::File, "file type detection");
            passed &= Check(listing.entries[0].size_bytes > 0, "allocated file size");
        }

        const tsw::ScanSummary summary = tsw::ComputeDirectorySize(directory.path);
        passed &= Check(summary.status == tsw::ScanStatus::Ready, "recursive scan status");
        passed &= Check(summary.size_bytes > 0, "recursive allocated size");
    }

    if (!passed) {
        return EXIT_FAILURE;
    }
    std::cout << "All filesystem tests passed." << std::endl;
    return EXIT_SUCCESS;
}
