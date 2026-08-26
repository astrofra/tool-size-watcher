#include "platform.h"

#include "windows_utf8.h"

#include <shlobj.h>
#include <shellapi.h>

namespace tsw {
namespace {

const wchar_t* const kSettingsKey = L"Software\\ToolSizeWatcher";
const wchar_t* const kExcludeNetworkVolumesValue = L"ExcludeNetworkVolumes";
const wchar_t* const kIncludeProtectedFoldersValue = L"IncludeProtectedFolders";

bool ReadBooleanSetting(const wchar_t* name, bool fallback) {
    DWORD value = fallback ? 1UL : 0UL;
    DWORD value_size = sizeof(value);
    const LSTATUS status = RegGetValueW(HKEY_CURRENT_USER, kSettingsKey, name, RRF_RT_REG_DWORD, NULL,
                                        &value, &value_size);
    return status == ERROR_SUCCESS ? value != 0 : fallback;
}

void WriteBooleanSetting(HKEY key, const wchar_t* name, bool value) {
    const DWORD stored_value = value ? 1UL : 0UL;
    RegSetValueExW(key, name, 0, REG_DWORD, reinterpret_cast<const BYTE*>(&stored_value),
                   sizeof(stored_value));
}

}  // namespace

const char* FileManagerDisplayName() {
    return "File Explorer";
}

bool OpenPathInFileManager(const std::string& path, bool is_directory) {
    const std::wstring wide_path = windows::Utf8ToWide(path);
    if (wide_path.empty()) {
        return false;
    }

    if (is_directory) {
        const HINSTANCE result = ShellExecuteW(NULL, L"open", wide_path.c_str(), NULL, NULL, SW_SHOWNORMAL);
        return reinterpret_cast<INT_PTR>(result) > 32;
    }

    PIDLIST_ABSOLUTE item = ILCreateFromPathW(wide_path.c_str());
    if (item == NULL) {
        return false;
    }

    PIDLIST_ABSOLUTE folder = ILCloneFull(item);
    if (folder == NULL || !ILRemoveLastID(folder)) {
        if (folder != NULL) {
            ILFree(folder);
        }
        ILFree(item);
        return false;
    }

    PCUITEMID_CHILD selected_item = ILFindLastID(item);
    const HRESULT result = SHOpenFolderAndSelectItems(folder, 1, &selected_item, 0);
    ILFree(folder);
    ILFree(item);
    return SUCCEEDED(result);
}

UserSettings LoadUserSettings() {
    UserSettings settings;
    settings.exclude_network_volumes =
        ReadBooleanSetting(kExcludeNetworkVolumesValue, settings.exclude_network_volumes);
    settings.include_protected_folders =
        ReadBooleanSetting(kIncludeProtectedFoldersValue, settings.include_protected_folders);
    return settings;
}

void SaveUserSettings(const UserSettings& settings) {
    HKEY key = NULL;
    DWORD disposition = 0;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kSettingsKey, 0, NULL, 0, KEY_SET_VALUE, NULL, &key,
                        &disposition) != ERROR_SUCCESS) {
        return;
    }

    WriteBooleanSetting(key, kExcludeNetworkVolumesValue, settings.exclude_network_volumes);
    WriteBooleanSetting(key, kIncludeProtectedFoldersValue, settings.include_protected_folders);
    RegCloseKey(key);
}

}  // namespace tsw
