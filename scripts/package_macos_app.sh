#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
DIST_DIR="${ROOT_DIR}/dist"
APP_NAME="ToolSizeWatcher.app"
BIN_NAME="ToolSizeWatcher"
PLIST_PATH="${ROOT_DIR}/resources/Info.plist"
APP_DIR="${DIST_DIR}/${APP_NAME}"

if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake is required but was not found in PATH." >&2
    exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Xcode Command Line Tools are required on macOS." >&2
    exit 1
fi

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --config Release

BIN_CANDIDATES=(
    "${BUILD_DIR}/${BIN_NAME}"
    "${BUILD_DIR}/Release/${BIN_NAME}"
)

BIN_PATH=""
for candidate in "${BIN_CANDIDATES[@]}"; do
    if [[ -x "${candidate}" ]]; then
        BIN_PATH="${candidate}"
        break
    fi
done

if [[ -z "${BIN_PATH}" ]]; then
    echo "Built executable not found." >&2
    exit 1
fi

rm -rf "${APP_DIR}"
mkdir -p "${APP_DIR}/Contents/MacOS" "${APP_DIR}/Contents/Resources"

cp "${PLIST_PATH}" "${APP_DIR}/Contents/Info.plist"
cp "${BIN_PATH}" "${APP_DIR}/Contents/MacOS/${BIN_NAME}"
chmod +x "${APP_DIR}/Contents/MacOS/${BIN_NAME}"

if [[ -f "${ROOT_DIR}/resources/AppIcon.icns" ]]; then
    cp "${ROOT_DIR}/resources/AppIcon.icns" "${APP_DIR}/Contents/Resources/AppIcon.icns"
fi

echo "Packaged: ${APP_DIR}"

