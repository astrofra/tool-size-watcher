#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
BUILD_TYPE="${BUILD_TYPE:-Release}"
PACKAGE_APP=1

if [[ "${1:-}" == "--binary-only" ]]; then
    PACKAGE_APP=0
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake is required but was not found in PATH." >&2
    exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Xcode Command Line Tools are required on macOS." >&2
    exit 1
fi

JOB_COUNT="$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" -j"${JOB_COUNT}"

if [[ "${PACKAGE_APP}" -eq 1 ]]; then
    SKIP_BUILD=1 BUILD_TYPE="${BUILD_TYPE}" "${ROOT_DIR}/scripts/package_macos_app.sh"
fi
