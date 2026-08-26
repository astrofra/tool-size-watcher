# Windows Port Addendum

This addendum extends the original macOS MVP specification to Windows while preserving the macOS target.
Unless stated below, the behavior and acceptance criteria in `macos-folder-size-browser-spec.md` apply to both platforms.

## Product parity

- The volumes view lists mounted macOS volumes or Windows drive letters, including mapped network drives.
- Direct children are displayed immediately and recursive directory sizes are computed by two background workers.
- Name and size sorting, keyboard navigation, rescanning, session-only caching, and the network-volume filter behave identically.
- `O` and the visible action open Finder on macOS or File Explorer on Windows. Files are revealed in their parent folder.
- Automatic scans skip protected personal folders until the user opens one explicitly or enables protected-folder scanning.
- Symbolic links, Windows junctions, and other reparse points are never traversed.

## Windows implementation

- Filesystem and volume access use Unicode Win32 APIs; application strings remain UTF-8 for Dear ImGui.
- Recursive scans use `FindFirstFileExW` / `FindNextFileW` and `GetCompressedFileSizeW` for allocated size.
- Long absolute paths are passed to filesystem APIs with the extended-length prefix and the executable declares `longPathAware`.
- Preferences are stored per user under `HKCU\Software\ToolSizeWatcher`.
- File Explorer integration uses `ShellExecuteW` for directories and `SHOpenFolderAndSelectItems` for files.
- The Windows GUI executable requests no elevation and uses a statically linked MSVC runtime.

## Build and packaging

Builds are native: produce the Windows artifact on Windows and the macOS artifact on macOS.
All source dependencies remain vendored, and neither target downloads dependencies during configuration.

Windows requirements:

- Windows 10 or later
- Visual Studio 2022 Build Tools with the Desktop development with C++ workload
- CMake 3.16 or later

The PowerShell packaging script creates both `dist/ToolSizeWatcher-windows-x64/` and
`dist/ToolSizeWatcher-windows-x64.zip`. The existing shell packaging flow continues to create
`dist/ToolSizeWatcher.app` on macOS.
