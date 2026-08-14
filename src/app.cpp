#include "app.h"

#include "macos_platform.h"

#include <imgui.h>

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace tsw {
namespace {

const ImGuiID kSortColumnName = 1;
const ImGuiID kSortColumnSize = 2;

std::string LowercaseAscii(const std::string& value) {
    std::string lower = value;
    for (std::size_t index = 0; index < lower.size(); ++index) {
        lower[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(lower[index])));
    }
    return lower;
}

bool PathIsInsideSubtree(const std::string& path, const std::string& root) {
    if (root == "/") {
        return !path.empty() && path[0] == '/';
    }
    if (path == root) {
        return true;
    }
    if (path.size() <= root.size()) {
        return false;
    }
    return path.compare(0, root.size(), root) == 0 && path[root.size()] == '/';
}

bool CompareEntriesByName(const EntryInfo& left, const EntryInfo& right, bool descending) {
    const std::string left_name = LowercaseAscii(left.name);
    const std::string right_name = LowercaseAscii(right.name);
    if (left_name != right_name) {
        return descending ? (left_name > right_name) : (left_name < right_name);
    }
    return left.full_path < right.full_path;
}

bool CompareEntriesBySize(const EntryInfo& left, const EntryInfo& right, bool descending) {
    if (left.size_bytes != right.size_bytes) {
        return descending ? (left.size_bytes > right.size_bytes) : (left.size_bytes < right.size_bytes);
    }
    return CompareEntriesByName(left, right, false);
}

bool ShouldShowApproximateSize(const EntryInfo& entry) {
    return entry.type == EntryType::Directory && entry.size_bytes > 0 && entry.status != ScanStatus::Ready;
}

std::string FormatEntrySize(const EntryInfo& entry) {
    const std::string size_text = FormatBytes(entry.size_bytes);
    if (ShouldShowApproximateSize(entry)) {
        return "~" + size_text;
    }
    return size_text;
}

}  // namespace

App::App()
    : scanner_(2) {
    scanner_.SetActiveEpoch(current_epoch_);
    RefreshVolumes();
}

App::~App() {
    BeginShutdown();
}

void App::BeginShutdown() {
    scanner_.RequestStop();
}

void App::Pump() {
    const std::vector<ScanEvent> events = scanner_.DrainEvents();
    for (std::size_t index = 0; index < events.size(); ++index) {
        const ScanEvent& event = events[index];
        if (event.epoch != current_epoch_) {
            continue;
        }

        if (event.kind == ScanEvent::Kind::Started) {
            queued_paths_.erase(event.path);
            scanning_paths_.insert(event.path);
            ApplyScanStatusToCache(event.path, ScanStatus::Scanning);
        } else if (event.kind == ScanEvent::Kind::Progress) {
            ApplyScanProgressToCache(event.path, event.summary.size_bytes);
        } else {
            queued_paths_.erase(event.path);
            scanning_paths_.erase(event.path);
            scan_cache_[event.path] = event.summary;
            ApplyScanSummaryToCache(event.path, event.summary);
        }
    }
}

void App::Render() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar;
    ImGui::Begin("Tool Size Watcher", NULL, flags);

    HandleShortcuts();
    Pump();

    const float status_bar_height = ImGui::GetFrameHeightWithSpacing();
    ImGui::BeginChild("MainContent", ImVec2(0.0f, -status_bar_height), false);
    if (view_mode_ == ViewMode::Volumes) {
        RenderVolumesView();
    } else {
        RenderBrowserView();
    }
    ImGui::EndChild();

    RenderStatusBar();

    ImGui::End();
}

void App::RefreshVolumes() {
    volumes_ = EnumerateVolumes();
    if (!selected_volume_mount_.empty()) {
        bool found = false;
        for (std::size_t index = 0; index < volumes_.size(); ++index) {
            if (volumes_[index].mount_path == selected_volume_mount_ && IsVolumeVisible(volumes_[index])) {
                found = true;
                break;
            }
        }
        if (!found) {
            selected_volume_mount_.clear();
        }
    }
}

void App::OpenVolume(const VolumeInfo& volume) {
    current_volume_root_ = volume.mount_path;
    selected_path_.clear();
    OpenDirectory(volume.mount_path, false);
}

void App::OpenDirectory(const std::string& path, bool force_reload) {
    if (path != current_path_) {
        AdvanceScanEpoch();
    }

    current_path_ = path;
    view_mode_ = ViewMode::Browser;

    DirectoryState& state = directory_cache_[path];
    if (force_reload || !state.loaded) {
        const DirectoryListing listing = ListDirectory(path);
        state.loaded = true;
        state.error_message = listing.error_message;
        state.entries = listing.entries;
    }

    PrepareDirectoryState(state);
    current_directory_dirty_ = true;
    EnsureSelectionIsVisible();
}

void App::NavigateUp() {
    if (view_mode_ != ViewMode::Browser) {
        return;
    }

    if (current_path_ == current_volume_root_) {
        AdvanceScanEpoch();
        view_mode_ = ViewMode::Volumes;
        current_path_.clear();
        selected_path_.clear();
        return;
    }

    OpenDirectory(ParentPath(current_path_), false);
}

void App::RescanCurrentDirectory() {
    if (view_mode_ != ViewMode::Browser || current_path_.empty()) {
        return;
    }

    AdvanceScanEpoch();
    RemoveCurrentSubtreeFromCache();
    selected_path_.clear();
    OpenDirectory(current_path_, true);
}

void App::OpenSelectedInFinder() const {
    const EntryInfo* entry = FindSelectedEntry();
    if (entry == NULL) {
        return;
    }

    const bool is_directory = entry->type == EntryType::Directory;
    OpenPathInFinder(entry->full_path, is_directory);
}

void App::EnterSelectedDirectory() {
    const EntryInfo* entry = FindSelectedEntry();
    if (entry == NULL || entry->type != EntryType::Directory) {
        return;
    }

    OpenDirectory(entry->full_path, false);
}

void App::RenderVolumesView() {
    ImGui::TextUnformatted("Mounted Storage Devices");
    ImGui::Spacing();

    if (ImGui::Button("Refresh Volumes")) {
        RefreshVolumes();
    }

    ImGui::SameLine();
    if (ImGui::Checkbox("Hide network volumes", &exclude_network_volumes_)) {
        if (FindVisibleSelectedVolume() == NULL) {
            selected_volume_mount_.clear();
        }
    }

    const VolumeInfo* selected_volume = FindVisibleSelectedVolume();
    if (selected_volume != NULL) {
        ImGui::SameLine();
        if (ImGui::Button("Browse Selected")) {
            OpenVolume(*selected_volume);
        }
    }

    ImGui::Spacing();
    int hidden_network_volume_count = 0;
    if (exclude_network_volumes_) {
        for (std::size_t index = 0; index < volumes_.size(); ++index) {
            if (volumes_[index].is_network) {
                ++hidden_network_volume_count;
            }
        }
        if (hidden_network_volume_count > 0) {
            ImGui::Text("Network volumes hidden: %d", hidden_network_volume_count);
            ImGui::Spacing();
        }
    }

    if (ImGui::BeginTable("volumes", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                         ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Mount Path");
        ImGui::TableSetupColumn("FS");
        ImGui::TableSetupColumn("Total");
        ImGui::TableSetupColumn("Free");
        ImGui::TableHeadersRow();

        for (std::size_t index = 0; index < volumes_.size(); ++index) {
            const VolumeInfo& volume = volumes_[index];
            if (!IsVolumeVisible(volume)) {
                continue;
            }
            const bool selected = volume.mount_path == selected_volume_mount_;

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            if (ImGui::Selectable(volume.name.c_str(), selected,
                                  ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                selected_volume_mount_ = volume.mount_path;
                if (ImGui::IsMouseDoubleClicked(0)) {
                    OpenVolume(volume);
                }
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", volume.mount_path.c_str());
            }

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(volume.mount_path.c_str());
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(volume.fs_type.c_str());
            ImGui::TableSetColumnIndex(3);
            ImGui::TextUnformatted(FormatBytes(volume.total_bytes).c_str());
            ImGui::TableSetColumnIndex(4);
            ImGui::TextUnformatted(FormatBytes(volume.free_bytes).c_str());
        }

        ImGui::EndTable();
    }
}

void App::RenderStatusBar() {
    const std::size_t active_workers = scanning_paths_.size();
    const std::size_t queued_folders = queued_paths_.size();
    const std::size_t worker_capacity = scanner_.WorkerCount();
    const std::size_t in_progress_folders = active_workers + queued_folders;

    std::size_t visible_volumes = 0;
    std::size_t hidden_network_volumes = 0;
    for (std::size_t index = 0; index < volumes_.size(); ++index) {
        if (IsVolumeVisible(volumes_[index])) {
            ++visible_volumes;
        } else if (volumes_[index].is_network) {
            ++hidden_network_volumes;
        }
    }

    std::string status_text;
    if (view_mode_ == ViewMode::Browser) {
        status_text = "Folders in progress: " + std::to_string(in_progress_folders) +
                      " | Waiting: " + std::to_string(queued_folders) +
                      " | Active workers: " + std::to_string(active_workers) + "/" +
                      std::to_string(worker_capacity);
    } else {
        status_text = "Visible volumes: " + std::to_string(visible_volumes) +
                      " | Hidden network volumes: " + std::to_string(hidden_network_volumes) +
                      " | Active workers: " + std::to_string(active_workers) + "/" +
                      std::to_string(worker_capacity);
    }

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.72f, 0.74f, 0.78f, 1.0f));
    ImGui::TextUnformatted(status_text.c_str());
    ImGui::PopStyleColor();
}

void App::RenderBrowserView() {
    DirectoryState* state = CurrentDirectoryState();
    if (state == NULL) {
        ImGui::TextUnformatted("No directory loaded.");
        return;
    }

    const bool has_selected_entry = FindSelectedEntry() != NULL;

    if (ImGui::Button("Volumes")) {
        AdvanceScanEpoch();
        view_mode_ = ViewMode::Volumes;
        current_path_.clear();
        selected_path_.clear();
    }
    ImGui::SameLine();
    if (ImGui::Button("Back")) {
        NavigateUp();
    }
    ImGui::SameLine();
    if (ImGui::Button("Rescan")) {
        RescanCurrentDirectory();
    }
    ImGui::SameLine();
    if (!has_selected_entry) {
        ImGui::BeginDisabled();
    }
    if (ImGui::Button("Open in Finder")) {
        OpenSelectedInFinder();
    }
    if (!has_selected_entry) {
        ImGui::EndDisabled();
    }

    ImGui::Spacing();
    ImGui::Text("Path: %s", current_path_.c_str());

    if (!state->error_message.empty() && state->entries.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "Cannot read directory: %s",
                           state->error_message.c_str());
    }

    ImGui::Spacing();
    if (ImGui::BeginTable("browser", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                         ImGuiTableFlags_Resizable | ImGuiTableFlags_Sortable |
                                         ImGuiTableFlags_SortMulti | ImGuiTableFlags_ScrollY |
                                         ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_DefaultSort, 0.0f, kSortColumnName);
        ImGui::TableSetupColumn("Type");
        ImGui::TableSetupColumn("Size", ImGuiTableColumnFlags_PreferSortDescending, 0.0f, kSortColumnSize);
        ImGui::TableSetupColumn("Status");
        ImGui::TableHeadersRow();

        ApplySortSpecs();
        SortCurrentDirectoryIfNeeded();

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(state->entries.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                EntryInfo& entry = state->entries[static_cast<std::size_t>(row)];
                const bool selected = entry.full_path == selected_path_;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                if (ImGui::Selectable(entry.name.c_str(), selected,
                                      ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowDoubleClick)) {
                    selected_path_ = entry.full_path;
                    if (entry.type == EntryType::Directory && ImGui::IsMouseDoubleClicked(0)) {
                        OpenDirectory(entry.full_path, false);
                    }
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", entry.full_path.c_str());
                }

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(EntryTypeToString(entry.type).c_str());

                ImGui::TableSetColumnIndex(2);
                const bool show_placeholder = entry.type == EntryType::Directory && entry.status == ScanStatus::Queued &&
                                              entry.size_bytes == 0;
                if (show_placeholder) {
                    ImGui::TextUnformatted("--");
                } else {
                    const std::string size_text = FormatEntrySize(entry);
                    ImGui::TextUnformatted(size_text.c_str());
                }

                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(ScanStatusToString(entry.status).c_str());
            }
        }

        ImGui::EndTable();
    }
}

void App::HandleShortcuts() {
    if (ImGui::IsKeyPressed(ImGuiKey_O, false)) {
        OpenSelectedInFinder();
    }

    if (view_mode_ == ViewMode::Volumes) {
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
            const VolumeInfo* selected_volume = FindVisibleSelectedVolume();
            if (selected_volume != NULL) {
                OpenVolume(*selected_volume);
            }
        }
        return;
    }

    if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
        RescanCurrentDirectory();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) {
        NavigateUp();
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
        EnterSelectedDirectory();
    }
}

void App::AdvanceScanEpoch() {
    ++current_epoch_;
    queued_paths_.clear();
    scanning_paths_.clear();
    scanner_.SetActiveEpoch(current_epoch_);
}

void App::PrepareDirectoryState(DirectoryState& state) {
    for (std::size_t index = 0; index < state.entries.size(); ++index) {
        EntryInfo& entry = state.entries[index];
        if (entry.type != EntryType::Directory) {
            entry.status = ScanStatus::Ready;
            continue;
        }

        const std::unordered_map<std::string, ScanSummary>::const_iterator cached = scan_cache_.find(entry.full_path);
        if (cached != scan_cache_.end()) {
            entry.size_bytes = cached->second.size_bytes;
            entry.status = cached->second.status;
            continue;
        }

        entry.size_bytes = 0;
        if (scanning_paths_.find(entry.full_path) != scanning_paths_.end()) {
            entry.status = ScanStatus::Scanning;
            continue;
        }
        if (queued_paths_.find(entry.full_path) != queued_paths_.end()) {
            entry.status = ScanStatus::Queued;
            continue;
        }

        const bool queued = scanner_.Enqueue(entry.full_path, current_epoch_);
        if (queued) {
            queued_paths_.insert(entry.full_path);
            entry.status = ScanStatus::Queued;
        } else {
            entry.status = ScanStatus::Scanning;
        }
    }
}

void App::SortCurrentDirectoryIfNeeded() {
    if (!current_directory_dirty_) {
        return;
    }

    DirectoryState* state = CurrentDirectoryState();
    if (state == NULL) {
        return;
    }

    if (sort_key_ == SortKey::Size) {
        std::sort(state->entries.begin(), state->entries.end(),
                  [this](const EntryInfo& left, const EntryInfo& right) {
                      return CompareEntriesBySize(left, right, sort_descending_);
                  });
    } else {
        std::sort(state->entries.begin(), state->entries.end(),
                  [this](const EntryInfo& left, const EntryInfo& right) {
                      return CompareEntriesByName(left, right, sort_descending_);
                  });
    }

    current_directory_dirty_ = false;
    EnsureSelectionIsVisible();
}

void App::ApplySortSpecs() {
    ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
    if (sort_specs == NULL || sort_specs->SpecsCount == 0) {
        return;
    }
    if (!sort_specs->SpecsDirty) {
        return;
    }

    const ImGuiTableColumnSortSpecs& spec = sort_specs->Specs[0];
    sort_key_ = (spec.ColumnUserID == kSortColumnSize) ? SortKey::Size : SortKey::Name;
    sort_descending_ = spec.SortDirection == ImGuiSortDirection_Descending;
    current_directory_dirty_ = true;
    sort_specs->SpecsDirty = false;
}

void App::ApplyScanStatusToCache(const std::string& path, ScanStatus status) {
    for (std::unordered_map<std::string, DirectoryState>::iterator dir_it = directory_cache_.begin();
         dir_it != directory_cache_.end(); ++dir_it) {
        DirectoryState& state = dir_it->second;
        for (std::size_t entry_index = 0; entry_index < state.entries.size(); ++entry_index) {
            EntryInfo& entry = state.entries[entry_index];
            if (entry.full_path == path) {
                entry.status = status;
            }
        }
    }

    current_directory_dirty_ = true;
}

void App::ApplyScanProgressToCache(const std::string& path, uint64_t size_bytes) {
    for (std::unordered_map<std::string, DirectoryState>::iterator dir_it = directory_cache_.begin();
         dir_it != directory_cache_.end(); ++dir_it) {
        DirectoryState& state = dir_it->second;
        for (std::size_t entry_index = 0; entry_index < state.entries.size(); ++entry_index) {
            EntryInfo& entry = state.entries[entry_index];
            if (entry.full_path == path) {
                entry.size_bytes = size_bytes;
                entry.status = ScanStatus::Scanning;
            }
        }
    }

    current_directory_dirty_ = true;
}

void App::ApplyScanSummaryToCache(const std::string& path, const ScanSummary& summary) {
    for (std::unordered_map<std::string, DirectoryState>::iterator dir_it = directory_cache_.begin();
         dir_it != directory_cache_.end(); ++dir_it) {
        DirectoryState& state = dir_it->second;
        for (std::size_t entry_index = 0; entry_index < state.entries.size(); ++entry_index) {
            EntryInfo& entry = state.entries[entry_index];
            if (entry.full_path == path) {
                entry.size_bytes = summary.size_bytes;
                entry.status = summary.status;
            }
        }
    }

    current_directory_dirty_ = true;
}

void App::RemoveCurrentSubtreeFromCache() {
    for (std::unordered_map<std::string, DirectoryState>::iterator it = directory_cache_.begin();
         it != directory_cache_.end();) {
        if (PathIsInsideSubtree(it->first, current_path_)) {
            it = directory_cache_.erase(it);
        } else {
            ++it;
        }
    }

    for (std::unordered_map<std::string, ScanSummary>::iterator it = scan_cache_.begin(); it != scan_cache_.end();) {
        if (PathIsInsideSubtree(it->first, current_path_)) {
            it = scan_cache_.erase(it);
        } else {
            ++it;
        }
    }
}

void App::EnsureSelectionIsVisible() {
    if (selected_path_.empty()) {
        return;
    }

    const DirectoryState* state = CurrentDirectoryState();
    if (state == NULL) {
        selected_path_.clear();
        return;
    }

    for (std::size_t index = 0; index < state->entries.size(); ++index) {
        if (state->entries[index].full_path == selected_path_) {
            return;
        }
    }

    selected_path_.clear();
}

bool App::IsVolumeVisible(const VolumeInfo& volume) const {
    return !exclude_network_volumes_ || !volume.is_network;
}

const VolumeInfo* App::FindVisibleSelectedVolume() const {
    if (selected_volume_mount_.empty()) {
        return NULL;
    }

    for (std::size_t index = 0; index < volumes_.size(); ++index) {
        const VolumeInfo& volume = volumes_[index];
        if (volume.mount_path == selected_volume_mount_ && IsVolumeVisible(volume)) {
            return &volume;
        }
    }

    return NULL;
}

DirectoryState* App::CurrentDirectoryState() {
    const std::unordered_map<std::string, DirectoryState>::iterator it = directory_cache_.find(current_path_);
    if (it == directory_cache_.end()) {
        return NULL;
    }
    return &it->second;
}

const DirectoryState* App::CurrentDirectoryState() const {
    const std::unordered_map<std::string, DirectoryState>::const_iterator it = directory_cache_.find(current_path_);
    if (it == directory_cache_.end()) {
        return NULL;
    }
    return &it->second;
}

const EntryInfo* App::FindSelectedEntry() const {
    const DirectoryState* state = CurrentDirectoryState();
    if (state == NULL || selected_path_.empty()) {
        return NULL;
    }

    for (std::size_t index = 0; index < state->entries.size(); ++index) {
        if (state->entries[index].full_path == selected_path_) {
            return &state->entries[index];
        }
    }
    return NULL;
}

}  // namespace tsw
