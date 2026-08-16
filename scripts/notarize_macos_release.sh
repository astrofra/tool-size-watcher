#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_PATH="${ROOT_DIR}/dist/ToolSizeWatcher.app"
NOTARY_PROFILE="${NOTARY_PROFILE:-}"
VERSION="${VERSION:-}"
ARCH="${ARCH:-}"
UPLOAD_ZIP="${UPLOAD_ZIP:-}"
RELEASE_ZIP="${RELEASE_ZIP:-}"

usage() {
    cat <<'EOF'
Usage: ./scripts/notarize_macos_release.sh [options]

Submits a signed ToolSizeWatcher.app for Apple notarization, staples the ticket,
runs Gatekeeper validation, then creates the final release ZIP and SHA-256 file.

Options:
  --notary-profile <name>  notarytool keychain profile to use. Required.
  --app <path>             App bundle to notarize. Default: dist/ToolSizeWatcher.app
  --version <value>        Release version for ZIP naming. Defaults to CFBundleShortVersionString.
  --arch <value>           Release architecture label. Defaults to binary inspection.
  --upload-zip <path>      ZIP path used for notarization upload.
  --release-zip <path>     Final public release ZIP path.
  --help                   Show this help text.

Environment:
  NOTARY_PROFILE           Same as --notary-profile.
  VERSION                  Same as --version.
  ARCH                     Same as --arch.
  UPLOAD_ZIP               Same as --upload-zip.
  RELEASE_ZIP              Same as --release-zip.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --notary-profile)
            if [[ $# -lt 2 ]]; then
                echo "--notary-profile requires a value." >&2
                exit 1
            fi
            NOTARY_PROFILE="$2"
            shift 2
            ;;
        --app)
            if [[ $# -lt 2 ]]; then
                echo "--app requires a value." >&2
                exit 1
            fi
            APP_PATH="$2"
            shift 2
            ;;
        --version)
            if [[ $# -lt 2 ]]; then
                echo "--version requires a value." >&2
                exit 1
            fi
            VERSION="$2"
            shift 2
            ;;
        --arch)
            if [[ $# -lt 2 ]]; then
                echo "--arch requires a value." >&2
                exit 1
            fi
            ARCH="$2"
            shift 2
            ;;
        --upload-zip)
            if [[ $# -lt 2 ]]; then
                echo "--upload-zip requires a value." >&2
                exit 1
            fi
            UPLOAD_ZIP="$2"
            shift 2
            ;;
        --release-zip)
            if [[ $# -lt 2 ]]; then
                echo "--release-zip requires a value." >&2
                exit 1
            fi
            RELEASE_ZIP="$2"
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            echo "Unknown option: $1" >&2
            usage >&2
            exit 1
            ;;
    esac
done

require_command() {
    if ! command -v "$1" >/dev/null 2>&1; then
        echo "$1 is required but was not found in PATH." >&2
        exit 1
    fi
}

require_command codesign
require_command ditto
require_command shasum
require_command spctl
require_command xcrun
require_command lipo

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Xcode Command Line Tools are required on macOS." >&2
    exit 1
fi

if [[ -z "${NOTARY_PROFILE}" ]]; then
    echo "A notarytool profile is required. Pass --notary-profile or set NOTARY_PROFILE." >&2
    exit 1
fi

if [[ ! -d "${APP_PATH}" ]]; then
    echo "App bundle not found: ${APP_PATH}" >&2
    exit 1
fi

APP_NAME="$(basename "${APP_PATH}")"
APP_STEM="${APP_NAME%.app}"
APP_DIR="$(cd "$(dirname "${APP_PATH}")" && pwd)"
APP_PATH="${APP_DIR}/${APP_NAME}"
PLIST_PATH="${APP_PATH}/Contents/Info.plist"
BIN_PATH="${APP_PATH}/Contents/MacOS/${APP_STEM}"

if [[ ! -f "${PLIST_PATH}" ]]; then
    echo "Info.plist not found: ${PLIST_PATH}" >&2
    exit 1
fi

if [[ ! -x "${BIN_PATH}" ]]; then
    echo "Main executable not found: ${BIN_PATH}" >&2
    exit 1
fi

if [[ -z "${VERSION}" ]]; then
    if [[ ! -x /usr/libexec/PlistBuddy ]]; then
        echo "PlistBuddy is required to auto-detect the version. Pass --version instead." >&2
        exit 1
    fi
    VERSION="$(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' "${PLIST_PATH}")"
fi

if [[ -z "${ARCH}" ]]; then
    case "$(lipo -archs "${BIN_PATH}")" in
        "arm64")
            ARCH="arm64"
            ;;
        "x86_64")
            ARCH="x86_64"
            ;;
        "x86_64 arm64"|"arm64 x86_64")
            ARCH="universal"
            ;;
        *)
            echo "Unable to infer release architecture from ${BIN_PATH}." >&2
            echo "Pass --arch explicitly." >&2
            exit 1
            ;;
    esac
fi

if [[ -z "${UPLOAD_ZIP}" ]]; then
    UPLOAD_ZIP="${APP_DIR}/${APP_STEM}-for-notarization.zip"
fi

if [[ -z "${RELEASE_ZIP}" ]]; then
    RELEASE_ZIP="${APP_DIR}/${APP_STEM}-macos-${ARCH}-v${VERSION}.zip"
fi

echo "App: ${APP_PATH}"
echo "Notary profile: ${NOTARY_PROFILE}"
echo "Version: ${VERSION}"
echo "Architecture: ${ARCH}"

echo
echo "Verifying signed app before notarization..."
codesign --verify --deep --strict --verbose=2 "${APP_PATH}"

echo
echo "Creating notarization upload ZIP..."
rm -f "${UPLOAD_ZIP}"
ditto -c -k --sequesterRsrc --keepParent "${APP_PATH}" "${UPLOAD_ZIP}"

echo
echo "Submitting to Apple notarization and waiting for completion..."
xcrun notarytool submit "${UPLOAD_ZIP}" \
    --keychain-profile "${NOTARY_PROFILE}" \
    --wait

echo
echo "Stapling notarization ticket..."
xcrun stapler staple "${APP_PATH}"
xcrun stapler validate "${APP_PATH}"

echo
echo "Running local Gatekeeper checks..."
spctl --assess --type execute --verbose=4 "${APP_PATH}"
codesign --verify --deep --strict --verbose=2 "${APP_PATH}"

echo
echo "Creating final release ZIP..."
rm -f "${RELEASE_ZIP}" "${RELEASE_ZIP}.sha256"
ditto -c -k --sequesterRsrc --keepParent "${APP_PATH}" "${RELEASE_ZIP}"
shasum -a 256 "${RELEASE_ZIP}" > "${RELEASE_ZIP}.sha256"

echo
echo "Release artifacts:"
echo "  Upload ZIP:  ${UPLOAD_ZIP}"
echo "  Release ZIP: ${RELEASE_ZIP}"
echo "  SHA-256:     ${RELEASE_ZIP}.sha256"
