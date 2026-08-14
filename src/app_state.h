#pragma once

#include <string>
#include <vector>

#include "filesystem.h"

namespace tsw {

enum class ViewMode {
    Volumes,
    Browser
};

enum class SortKey {
    Name,
    Size
};

struct DirectoryState {
    bool loaded = false;
    std::string error_message;
    std::vector<EntryInfo> entries;
};

}  // namespace tsw

