#pragma once

#include <string>

#include "app_state.h"

namespace tsw {

const char* FileManagerDisplayName();
bool OpenPathInFileManager(const std::string& path, bool is_directory);
UserSettings LoadUserSettings();
void SaveUserSettings(const UserSettings& settings);

}  // namespace tsw
