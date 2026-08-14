# Tool Size Watcher

Desktop macOS utility to browse mounted storage devices, inspect folder contents, and compute recursive folder sizes in a Dear ImGui GUI.

By default, automatic scans skip protected macOS locations such as `Desktop`, `Documents`, `Downloads`, and the Photos Library until you explicitly open them or enable protected-folder scanning. User preferences are persisted with standard macOS per-user preferences.

## Build

Requirements:

- Xcode Command Line Tools or Xcode
- `cmake`

Dependencies are vendored in `third_party/`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Or, for the usual local workflow:

```bash
./rebuild.sh
```

This rebuilds the executable and regenerates `dist/ToolSizeWatcher.app`.

## Package as `.app`

```bash
./scripts/package_macos_app.sh
```

The packaging script outputs `dist/ToolSizeWatcher.app`.
