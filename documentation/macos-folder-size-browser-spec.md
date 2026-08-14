# macOS Folder Size Browser - Product and Technical Specification

## 1. Purpose

Build a desktop GUI tool for macOS that:

- Lists all storage devices visible to the machine.
- Lets the user enter any device or folder.
- Lists the direct contents of the current location.
- Shows the occupied size next to each item.
- Computes folder sizes recursively.
- Opens the selected item in Finder when the user presses `O`.
- Recomputes everything from scratch on every application launch.

This document defines the MVP scope, technical choices, and a development plan.

## 2. Scope

### In Scope

- macOS-only first release.
- Native desktop application packaged as a `.app`.
- GUI built with Dear ImGui.
- C++11 implementation.
- Recursive size calculation for directories.
- Session-only in-memory state.
- Keyboard and mouse navigation.
- Finder integration for the selected item.
- Sorting the current folder view by name or size.
- Maximum practical vendoring of third-party dependencies.

### Out of Scope

- Windows support in the first release.
- Persistent cache or database.
- Background indexing across reboots.
- File deletion, move, rename, or cleanup actions.
- Advanced visualizations such as treemaps or charts.
- Network storage special handling beyond standard mounted volumes.

## 3. Product Summary

The application is a lightweight folder size browser.

At startup, it shows a list of mounted storage devices. After the user selects a device, the main panel shows the device root contents. Each row displays:

- Name
- Kind (`Folder` or `File`)
- Size
- Scan status when relevant

For files, the size is the file size.

For folders, the size is the total recursive size of all descendant files under that folder.

The user can move into any folder, inspect its direct children, go back to the parent folder, and press `O` to reveal the selected item in Finder.

The current folder view can be sorted by name or by size.

## 4. Primary Goals

1. Keep the UI responsive while scanning.
2. Show useful folder sizes without persisting data on disk.
3. Keep the code simple and readable.
4. Use a portable GUI stack centered on Dear ImGui.
5. Produce a distributable macOS `.app` with a shell packaging script.

## 5. Non-Functional Constraints

- Language: `C++11`
- GUI: `Dear ImGui`
- Style: simple code, limited defensive code, clear control flow
- Persistence: none on disk
- Recalculation policy: full recomputation on each launch
- Performance target: usable on large folder trees without freezing the UI

## 6. UX Requirements

### 6.1 Main Views

The application has two main states:

1. Device list
2. Folder browser

### 6.2 Device List

Show a table of mounted storage volumes with:

- Volume name
- Mount point
- Filesystem type
- Optional total space
- Optional free space

The list should hide pseudo-filesystems that are not real user storage targets, for example `devfs`, `autofs`, and similar system mounts.

The device list must also provide a checkbox to exclude network volumes from the visible list. This checkbox is enabled by default.

### 6.3 Folder Browser

Show:

- Current path at the top
- A `Back` action to go to the parent directory
- A `Rescan` action for the current directory
- A scrollable table for direct children

Each row should display:

- Name
- Type
- Human-readable size
- Status

The table must support sorting by:

- `Name`
- `Size`

Status values for folders may include:

- `Queued`
- `Scanning`
- `Ready`
- `Error`
- `Denied`

### 6.4 Navigation

- Double-click or `Enter` on a folder enters that folder.
- `Backspace` or a visible `Back` button goes to the parent folder.
- Arrow keys move selection.
- Pressing `O` opens the selected item in Finder.
- Pressing `R` rescans the current folder.

Mouse support is expected, but the UI must remain fully usable from the keyboard.

### 6.5 Sorting Behavior

- Default sort is `Name`, ascending.
- Clicking the `Name` column header sorts by name.
- Clicking the `Size` column header sorts by size.
- Repeating the same sort action toggles ascending or descending order.
- Sorting applies to the current folder direct children only.
- While folder scans are still running, size-based sort uses the latest known size values and updates the displayed order as results arrive.

### 6.6 Size Formatting

Sizes must be displayed in a human-readable format using binary steps:

- `B`
- `KB`
- `MB`
- `GB`
- `TB`

Base is `1024`.

Examples:

- `512 B`
- `12.4 KB`
- `83.7 MB`
- `1.9 GB`

## 7. Functional Requirements

### 7.1 Device Enumeration

On startup, the application must enumerate mounted filesystems and build a filtered device list.

macOS implementation should use system mount information, for example:

- `getmntinfo`
- `statfs`

Filtering rules:

- Include user-visible mounted storage volumes.
- Include the system root volume `/`.
- Exclude pseudo-filesystems and ephemeral internal mounts that are not meaningful to browse in this tool.

### 7.2 Directory Listing

For the current path, list direct children only:

- Subdirectories
- Files

Do not flatten descendants into the view.

### 7.3 Sorting

The folder browser must support sorting the visible entries by:

- Name
- Size

Rules:

- Sorting acts on the current folder listing only.
- Name sort is lexicographic and case-insensitive for display purposes.
- Size sort uses exact file sizes for files and the latest computed recursive sizes for directories.
- If multiple entries compare equal, fall back to name ascending.
- Entries whose directory size is not yet computed may temporarily appear as size `0` or as an explicit pending state, but the behavior must be consistent.

### 7.4 Recursive Size Calculation

For every directory in the current listing, compute:

- The total size of all descendant files
- Using recursive traversal of the subtree rooted at that directory

Rules:

- Do not follow symlinked directories.
- Symlink files may be counted by their link entry size or skipped; the chosen behavior must be consistent and documented in code comments.
- If an entry disappears during scanning, skip it and mark the folder status as partial or error.
- If permission is denied for a subtree, continue scanning other siblings and mark the affected directory accordingly.

### 7.5 Rescan Behavior

- On application start, no previous session data is loaded.
- In-memory scan results exist only for the current session.
- When the user triggers `Rescan`, the current folder subtree cache in memory is discarded and recomputed.
- When the application quits, all computed data is lost.

### 7.6 Open in Finder

When the user presses `O`:

- If a row is selected, open that path in Finder.
- If no row is selected, do nothing.

Recommended macOS implementation:

- `open <path>`

Optional improvement:

- `open -R <path>` for files if reveal behavior is preferred over opening.

For the MVP, simple `open` is acceptable.

## 8. Recommended Technical Design

### 8.1 Chosen Stack

- Language: `C++11`
- GUI: `Dear ImGui`
- Windowing/input: `GLFW`
- Rendering backend: `OpenGL 3`
- Platform APIs: POSIX + small macOS-specific helpers
- Build system: `CMake`
- Third-party delivery model: vendored source snapshots committed into the repository

### 8.2 Why This Stack

`Dear ImGui + GLFW + OpenGL3` is a pragmatic MVP choice:

- Portable enough for future Windows support.
- Much simpler than a custom Cocoa UI.
- Easy to package as a normal macOS app.
- Keeps the rendering and input layer separate from scan logic.

OpenGL is deprecated on macOS, but still acceptable for a simple desktop utility MVP. If long-term macOS-native support becomes a priority, the renderer can later be moved to Metal without changing most of the application logic.

### 8.3 High-Level Modules

Suggested source layout:

```text
/documentation
/src
  main.cpp
  app.cpp
  app_state.h
  scanner.cpp
  scanner.h
  filesystem.cpp
  filesystem.h
  macos_platform.mm
  ui_browser.cpp
  ui_browser.h
/third_party
  imgui/
  glfw/
  licenses/
/scripts
  package_macos_app.sh
/resources
  Info.plist
  AppIcon.icns    # optional for MVP
/CMakeLists.txt
```

Module responsibilities:

- `filesystem.*`: directory enumeration, mount enumeration, size formatting
- `scanner.*`: background scan jobs and recursive folder size computation
- `ui_browser.*`: Dear ImGui rendering and input handling
- `macos_platform.mm`: small macOS-specific helpers if needed
- `app.*`: application state and navigation

### 8.4 Data Model

Suggested core structures:

```cpp
struct VolumeInfo {
    std::string name;
    std::string mount_path;
    std::string fs_type;
    uint64_t total_bytes;
    uint64_t free_bytes;
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
    EntryType type;
    uint64_t size_bytes;
    ScanStatus status;
};
```

Notes:

- `size_bytes` is exact for files.
- `size_bytes` is recursive aggregate size for directories.
- Results are kept in memory only.

### 8.5 Scanning Strategy

The scan must not block rendering.

Recommended approach:

- The UI thread enumerates the current folder direct children.
- For each child directory, enqueue a background job to compute recursive size.
- A worker thread pool of size `1` or `2` is enough for the MVP.
- As jobs complete, update the corresponding row in memory.

This keeps the implementation simple while preserving responsiveness.

The current folder view should appear immediately, even if some directory sizes are still pending.

### 8.6 Traversal Rules

Use POSIX traversal rather than `std::filesystem`, because the target is `C++11`.

Recommended APIs:

- `opendir`
- `readdir`
- `lstat`
- `stat`
- `closedir`

Traversal rules:

- Skip `.` and `..`
- Do not follow directory symlinks
- Count file sizes from `st_size`
- Recursively traverse only true directories

### 8.7 Error Handling Philosophy

Keep error handling simple and local:

- Per-entry failures must not abort the whole scan.
- Directory permission failures should set status to `Denied`.
- Unexpected I/O failures should set status to `Error`.
- The app should log basic failures to stderr in debug builds.

Avoid large error frameworks, exception-heavy control flow, or persistent recovery mechanisms.

## 9. macOS Integration Details

### 9.1 Finder Open

Preferred simple implementation:

```cpp
std::string cmd = "open \"" + escaped_path + "\"";
std::system(cmd.c_str());
```

Safer improvement for later:

- Use `posix_spawn`
- Or a tiny Objective-C++ bridge to `NSWorkspace`

For the MVP, a minimal implementation is acceptable if path escaping is handled correctly.

### 9.2 `.app` Bundle

The output bundle should look like:

```text
ToolSizeWatcher.app/
  Contents/
    Info.plist
    MacOS/
      ToolSizeWatcher
    Resources/
      AppIcon.icns   # optional for MVP
```

### 9.3 Packaging Script

Provide a shell script:

```text
scripts/package_macos_app.sh
```

Responsibilities:

1. Verify minimal host prerequisites are present.
2. Build the executable with CMake.
3. Create the `.app` bundle structure.
4. Copy the executable into `Contents/MacOS/`.
5. Copy `Info.plist` into `Contents/`.
6. Copy optional resources into `Contents/Resources/`.
7. Output the final bundle into `dist/`.

Expected minimal host prerequisites:

- Apple Clang via Xcode Command Line Tools or Xcode
- `cmake`

The script must not download dependencies from the network.

Optional later steps:

- Codesign
- DMG creation

For the MVP, unsigned `.app` output is enough.

## 10. Build and Dependency Strategy

Recommended approach:

- Vendor Dear ImGui in `third_party/imgui`
- Vendor GLFW in `third_party/glfw`
- Commit the actual third-party source snapshots into the repository
- Build everything through CMake
- Link only against macOS system frameworks that are already present on the machine

Dependency policy:

- No Homebrew dependency must be required to build the app.
- No runtime dependency manager must be required.
- No configure-time download step must exist.
- Do not use `FetchContent`, `ExternalProject`, or similar network-based CMake flows for the MVP.
- Do not rely on git submodules for the MVP, because they make first-time checkout and build less reliable.
- Prefer plain source vendoring or `git subtree`-style import so a fresh clone is buildable immediately.
- Keep third-party version numbers pinned and documented in the repository.
- Keep third-party licenses copied into `third_party/licenses/`.

Expected external dependencies on a clean macOS machine:

- Apple-provided system frameworks such as `Cocoa`, `IOKit`, `CoreVideo`, and `OpenGL`
- Apple Clang toolchain
- `cmake`

Everything else should come from the repository itself.

Reasons:

- Avoid requiring Homebrew on the target machine
- Keep the build reproducible
- Make a fresh clone compile with minimal setup
- Keep packaging script straightforward

Minimum build artifacts:

- One executable
- One `.app` bundle

Recommended CMake structure:

- Use `add_subdirectory(third_party/glfw)` for GLFW
- Compile Dear ImGui sources directly into the app target or a small local static library target
- Keep all include paths local to the repository
- Avoid optional third-party features that introduce extra platform dependencies

## 11. Acceptance Criteria

The MVP is complete when all of the following are true:

1. Launching the app shows a list of mounted storage devices.
2. Selecting a device opens a folder browser for its root.
3. The folder browser lists direct children of the current path.
4. The user can sort the current folder view by name or by size.
5. File sizes are shown immediately.
6. Folder sizes are computed recursively and appear asynchronously.
7. The UI stays responsive during large scans.
8. The user can enter folders and go back to parents.
9. Pressing `O` opens the selected item in Finder.
10. Relaunching the app starts with no cached scan data.
11. A shell script can produce a runnable `.app`.
12. A fresh clone builds without Homebrew and without downloading extra libraries.

## 12. Risks and Mitigations

### Large Trees

Risk:

- Recursive scans can take a long time on large disks.

Mitigation:

- Compute sizes in background workers.
- Show `Scanning` state per directory.
- Allow manual rescan only for the current subtree.

### Permission Errors

Risk:

- Some directories are unreadable.

Mitigation:

- Mark the affected row as `Denied`.
- Continue scanning the rest.

### macOS Volume Noise

Risk:

- Raw mount enumeration includes system mounts that clutter the UI.

Mitigation:

- Filter filesystem types and mount flags aggressively for the first release.
- Keep the filter logic isolated for easy tuning.

### OpenGL on macOS

Risk:

- OpenGL is deprecated.

Mitigation:

- Accept it for MVP simplicity.
- Keep scan and UI logic separated so the renderer can be replaced later.

## 13. Development Plan

### Phase 1 - Project Skeleton

- Create `CMakeLists.txt`
- Vendor Dear ImGui and GLFW source snapshots into `third_party/`
- Wire Dear ImGui and GLFW directly in CMake without package-manager assumptions
- Open a basic Dear ImGui window on macOS
- Add a minimal app state object

Deliverable:

- App launches as a desktop window from a repository-contained dependency set.

### Phase 2 - Device Enumeration

- Implement mounted volume enumeration
- Filter out pseudo-filesystems
- Render the device list table
- Support selection and transition into a chosen volume

Deliverable:

- App shows a clean list of user-meaningful storage volumes.

### Phase 3 - Folder Listing

- Implement direct child listing for a path
- Render a table with files and folders
- Add selection, double-click, `Enter`, and `Back`
- Add name-based sorting in the table UI

Deliverable:

- User can browse folders interactively.

### Phase 4 - Recursive Size Scanner

- Implement recursive size computation with POSIX traversal
- Add worker thread execution
- Show per-row scan state updates
- Format sizes in readable units
- Add size-based sorting and live reordering as scan results arrive

Deliverable:

- Folder sizes fill in asynchronously without blocking the UI.

### Phase 5 - Finder Integration and Rescan

- Implement `O` shortcut
- Implement current-folder `Rescan`
- Clear and rebuild in-memory results for the current subtree

Deliverable:

- User can reveal selected items in Finder and refresh scans.

### Phase 6 - Packaging

- Add `Info.plist`
- Write `scripts/package_macos_app.sh`
- Build and bundle the executable into `.app`
- Validate launch from Finder

Deliverable:

- Runnable `.app` produced from a shell script.

### Phase 7 - Polish

- Validate behavior on large folders
- Tune filtering of mounted volumes
- Improve status text and minor UX details
- Add optional icon

Deliverable:

- MVP ready for regular local use.

## 14. Suggested First Implementation Order

If development starts immediately, the best order is:

1. Window + Dear ImGui bootstrap
2. Volume enumeration
3. Folder navigation
4. Recursive scanner
5. Finder open shortcut
6. Packaging script

This order minimizes risk and exposes platform issues early.

## 15. Notes for Future Versions

Possible later additions:

- Windows Explorer integration
- Breadcrumb navigation
- Progress bar per scan
- Cancellation of long scans
- Treemap view
- Optional persistent cache
- Multi-pane comparison between folders

These are intentionally excluded from the MVP.
