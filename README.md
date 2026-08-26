# Tool Size Watcher

Cross-platform desktop utility for Windows and macOS that browses mounted storage, displays direct folder contents,
and computes occupied folder sizes recursively in a responsive Dear ImGui interface.

Automatic scans skip protected personal locations such as Desktop, Documents, and Downloads until they are opened
explicitly or protected-folder scanning is enabled. Settings are persisted per user in macOS preferences or the
Windows registry; scan results remain in memory and are rebuilt on every launch.

## Features

- Local, removable, and network volume discovery
- Asynchronous recursive size scans with live progress
- Name and size sorting
- Mouse and keyboard navigation (`Enter`, `Backspace`, `R`, and `O`)
- Finder integration on macOS and File Explorer integration on Windows
- UTF-8 filenames and long-path-aware Windows filesystem access
- Vendored GLFW and Dear ImGui dependencies; no configure-time downloads

## Build on Windows

Requirements:

- Windows 10 or later
- Visual Studio 2022 Build Tools with **Desktop development with C++**
- CMake 3.16 or later

From PowerShell:

```powershell
cmake -S . -B build/windows -A x64
cmake --build build/windows --config Release --parallel
```

Or build and create a distributable ZIP in `dist/`:

```powershell
.\rebuild.ps1
```

Pass `-BinaryOnly` to skip packaging or `-Configuration Debug` for a debug build.

To include and run the filesystem smoke tests:

```powershell
cmake -S . -B build/windows-tests -A x64 -DTOOL_SIZE_WATCHER_BUILD_TESTS=ON
cmake --build build/windows-tests --config Release --parallel
ctest --test-dir build/windows-tests -C Release --output-on-failure
```

## Build on macOS

Requirements:

- macOS 11 or later
- Xcode Command Line Tools or Xcode
- CMake 3.16 or later

```bash
cmake -S . -B build/macos -DCMAKE_BUILD_TYPE=Release
cmake --build build/macos --config Release --parallel
```

Or build and regenerate `dist/ToolSizeWatcher.app`:

```bash
./rebuild.sh
```

## Packaging only

```powershell
.\scripts\package_windows.ps1
```

```bash
./scripts/package_macos_app.sh
```

The original product specification is in
[`documentation/macos-folder-size-browser-spec.md`](documentation/macos-folder-size-browser-spec.md), with Windows
parity and implementation details in [`documentation/windows-port-spec.md`](documentation/windows-port-spec.md).
