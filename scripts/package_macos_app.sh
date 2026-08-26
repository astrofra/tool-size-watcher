#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build/macos"
DIST_DIR="${ROOT_DIR}/dist"
APP_NAME="ToolSizeWatcher.app"
BIN_NAME="ToolSizeWatcher"
PLIST_PATH="${ROOT_DIR}/resources/Info.plist"
APP_DIR="${DIST_DIR}/${APP_NAME}"
ICON_SOURCE_PNG="${ROOT_DIR}/design/main-icon.png"
ICONSET_DIR="${BUILD_DIR}/AppIcon.iconset"
GENERATED_ICNS="${BUILD_DIR}/AppIcon.icns"
BUILD_TYPE="${BUILD_TYPE:-Release}"
SKIP_BUILD="${SKIP_BUILD:-0}"

if ! command -v cmake >/dev/null 2>&1; then
    echo "cmake is required but was not found in PATH." >&2
    exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Xcode Command Line Tools are required on macOS." >&2
    exit 1
fi

if ! command -v sips >/dev/null 2>&1; then
    echo "sips is required but was not found in PATH." >&2
    exit 1
fi

if ! command -v iconutil >/dev/null 2>&1; then
    echo "iconutil is required but was not found in PATH." >&2
    exit 1
fi

if [[ "${SKIP_BUILD}" != "1" ]]; then
    cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}"
fi

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

if [[ -f "${ICON_SOURCE_PNG}" ]]; then
    rm -rf "${ICONSET_DIR}"
    mkdir -p "${ICONSET_DIR}"

    for size in 16 32 128 256 512; do
        sips -z "${size}" "${size}" "${ICON_SOURCE_PNG}" --out "${ICONSET_DIR}/icon_${size}x${size}.png" >/dev/null
    done

    for size in 16 32 128 256 512; do
        retina_size=$((size * 2))
        sips -z "${retina_size}" "${retina_size}" "${ICON_SOURCE_PNG}" \
            --out "${ICONSET_DIR}/icon_${size}x${size}@2x.png" >/dev/null
    done

    rm -f "${GENERATED_ICNS}"
    iconutil -c icns "${ICONSET_DIR}" -o "${GENERATED_ICNS}"
    cp "${GENERATED_ICNS}" "${APP_DIR}/Contents/Resources/AppIcon.icns"
elif [[ -f "${ROOT_DIR}/resources/AppIcon.icns" ]]; then
    cp "${ROOT_DIR}/resources/AppIcon.icns" "${APP_DIR}/Contents/Resources/AppIcon.icns"
fi

echo "Packaged: ${APP_DIR}"
