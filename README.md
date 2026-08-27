# Tool Size Watcher

Tool Size Watcher is a desktop utility for Windows and macOS that helps you inspect mounted drives, browse folders, and see recursive folder sizes without blocking the UI.

## Download

Download the latest builds from [GitHub Releases](https://github.com/astrofra/tool-size-watcher/releases).

## What It Does

- Lists local, removable, and network volumes
- Shows direct folder contents with recursive folder sizes
- Scans folders asynchronously with live progress updates
- Sorts entries by name or size
- Opens the selected item in Finder on macOS or File Explorer on Windows

## How To Use

At launch, the app shows the storage volumes available on your machine. Open a volume or folder to inspect its direct contents while folder sizes are computed in the background.

Some protected personal locations such as `Desktop`, `Documents`, and `Downloads` are skipped during automatic scans until you open them explicitly or enable protected-folder scanning. Settings are stored per user, but scan results are rebuilt on every launch.

## Navigation

- `Enter`: open the selected folder
- `Backspace`: go to the parent folder
- `R`: rescan the current folder
- `O`: open the selected item in the platform file manager
- Mouse and keyboard navigation are both supported

## Supported Platforms

- Windows 10 or later
- macOS 11 or later

## Build From Source

### Windows

Requirements:

- Visual Studio 2022 Build Tools with `Desktop development with C++`
- CMake 3.16 or later

Build from PowerShell:

```powershell
cmake -S . -B build/windows -A x64
cmake --build build/windows --config Release --parallel
```

Build and create a distributable ZIP in `dist/`:

```powershell
.\rebuild.ps1
```

Useful options:

- `-BinaryOnly` skips packaging
- `-Configuration Debug` creates a debug build

### macOS

Requirements:

- Xcode Command Line Tools or Xcode
- CMake 3.16 or later

```bash
cmake -S . -B build/macos -DCMAKE_BUILD_TYPE=Release
cmake --build build/macos --config Release --parallel
```

Build and regenerate `dist/ToolSizeWatcher.app`:

```bash
./rebuild.sh
```

## Tests

Build and run the filesystem smoke tests on Windows:

```powershell
cmake -S . -B build/windows-tests -A x64 -DTOOL_SIZE_WATCHER_BUILD_TESTS=ON
cmake --build build/windows-tests --config Release --parallel
ctest --test-dir build/windows-tests -C Release --output-on-failure
```

## Packaging

Windows:

```powershell
.\scripts\package_windows.ps1
```

macOS:

```bash
./scripts/package_macos_app.sh
```

## Additional Documentation

- [Original product specification](documentation/macos-folder-size-browser-spec.md)
- [Windows port specification](documentation/windows-port-spec.md)
- [Third-party dependencies](documentation/third-party-dependencies.md)
