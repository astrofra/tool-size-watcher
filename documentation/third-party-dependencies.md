# Vendored Third-Party Dependencies

This repository vendors the third-party sources required to build the macOS app locally without Homebrew and without configure-time downloads.

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

The only expected external build prerequisites on macOS are:

- Apple Clang via Xcode or Xcode Command Line Tools
- `cmake`

Everything else required by the app is expected to come from the repository itself or from Apple system frameworks already present on macOS.

