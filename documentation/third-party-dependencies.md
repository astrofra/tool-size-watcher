# Vendored Third-Party Dependencies

This repository vendors the third-party sources required to build the Windows and macOS apps without a package manager or configure-time downloads.

## Dear ImGui

- Version: `v1.92.9`
- Source: `https://github.com/ocornut/imgui`
- Vendored path: `third_party/imgui`
- License copy: `third_party/licenses/imgui-LICENSE.txt`

## GLFW

- Version: `3.5.1`
- Source: `https://github.com/glfw/glfw`
- Vendored path: `third_party/glfw`
- License copy: `third_party/licenses/glfw-LICENSE.md`

## Host Requirements

The expected external build prerequisites are:

- macOS: Apple Clang via Xcode or Xcode Command Line Tools, plus `cmake`
- Windows: Visual Studio 2022 Build Tools with the Desktop C++ workload, plus `cmake`

Everything else required by the app comes from the repository or from system frameworks and libraries already present on the target OS.
