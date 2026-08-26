#pragma once

#if !defined(_WIN32)
#error "windows_utf8.h is only available on Windows"
#endif

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <string>

namespace tsw {
namespace windows {

inline std::wstring Utf8ToWide(const std::string& value) {
    if (value.empty()) {
        return std::wstring();
    }

    const int required = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(),
                                              static_cast<int>(value.size()), NULL, 0);
    if (required <= 0) {
        return std::wstring();
    }

    std::wstring converted(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        &converted[0], required);
    return converted;
}

inline std::string WideToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return std::string();
    }

    const int required = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(),
                                              static_cast<int>(value.size()), NULL, 0, NULL, NULL);
    if (required <= 0) {
        return std::string();
    }

    std::string converted(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value.data(), static_cast<int>(value.size()),
                        &converted[0], required, NULL, NULL);
    return converted;
}

inline std::wstring ToExtendedLengthPath(const std::wstring& path) {
    if (path.compare(0, 4, L"\\\\?\\") == 0) {
        return path;
    }
    if (path.compare(0, 2, L"\\\\") == 0) {
        return L"\\\\?\\UNC\\" + path.substr(2);
    }
    if (path.size() >= 3 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/')) {
        return L"\\\\?\\" + path;
    }
    return path;
}

inline std::wstring ToExtendedLengthPath(const std::string& utf8_path) {
    return ToExtendedLengthPath(Utf8ToWide(utf8_path));
}

}  // namespace windows
}  // namespace tsw
