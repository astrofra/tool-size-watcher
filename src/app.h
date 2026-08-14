#pragma once

#include <stdint.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "app_state.h"
#include "scanner.h"

namespace tsw {

class App {
public:
    App();

    void Pump();
    void Render();

private:
    void RefreshVolumes();
    void OpenVolume(const VolumeInfo& volume);
    void OpenDirectory(const std::string& path, bool force_reload);
    void NavigateUp();
    void RescanCurrentDirectory();
    void OpenSelectedInFinder() const;
    void EnterSelectedDirectory();

    void RenderVolumesView();
    void RenderBrowserView();
    void HandleShortcuts();

    void AdvanceScanEpoch();
    void PrepareDirectoryState(DirectoryState& state);
    void SortCurrentDirectoryIfNeeded();
    void ApplySortSpecs();
    void ApplyScanStatusToCache(const std::string& path, ScanStatus status);
    void ApplyScanSummaryToCache(const std::string& path, const ScanSummary& summary);

    void RemoveCurrentSubtreeFromCache();
    void EnsureSelectionIsVisible();
    bool IsVolumeVisible(const VolumeInfo& volume) const;
    const VolumeInfo* FindVisibleSelectedVolume() const;

    DirectoryState* CurrentDirectoryState();
    const DirectoryState* CurrentDirectoryState() const;
    const EntryInfo* FindSelectedEntry() const;

    std::vector<VolumeInfo> volumes_;
    ViewMode view_mode_ = ViewMode::Volumes;
    std::string current_volume_root_;
    std::string current_path_;
    std::string selected_volume_mount_;
    std::string selected_path_;
    std::unordered_map<std::string, DirectoryState> directory_cache_;
    std::unordered_map<std::string, ScanSummary> scan_cache_;
    std::unordered_set<std::string> queued_paths_;
    std::unordered_set<std::string> scanning_paths_;
    SortKey sort_key_ = SortKey::Name;
    bool sort_descending_ = false;
    bool exclude_network_volumes_ = true;
    bool current_directory_dirty_ = true;
    uint64_t current_epoch_ = 1;
    ScanScheduler scanner_;
};

}  // namespace tsw
