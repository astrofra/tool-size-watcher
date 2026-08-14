# Tool Size Watcher

Desktop macOS utility to browse mounted storage devices, inspect folder contents, and compute recursive folder sizes in a Dear ImGui GUI.

## Build

Requirements:

- Xcode Command Line Tools or Xcode
- `cmake`

Dependencies are vendored in `third_party/`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Package as `.app`

```bash
./scripts/package_macos_app.sh
```

The packaging script outputs `dist/ToolSizeWatcher.app`.
