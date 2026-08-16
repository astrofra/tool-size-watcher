#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APP_PATH="${ROOT_DIR}/dist/ToolSizeWatcher.app"
BUILD_TYPE="${BUILD_TYPE:-Release}"
IDENTITY="${IDENTITY:-}"
SKIP_BUILD=0

usage() {
    cat <<'EOF'
Usage: ./scripts/sign_macos_app.sh [options]

Builds and packages ToolSizeWatcher.app unless --skip-build is used, then signs
the app with Developer ID, enables Hardened Runtime, and verifies the result.

Options:
  --identity <name>    Exact codesigning identity to use.
  --app <path>         App bundle to sign. Default: dist/ToolSizeWatcher.app
  --build-type <type>  CMake build type. Default: Release
  --skip-build         Sign the existing app bundle without rebuilding.
  --help               Show this help text.

Environment:
  IDENTITY             Same as --identity.
  BUILD_TYPE           Same as --build-type.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --identity)
            if [[ $# -lt 2 ]]; then
                echo "--identity requires a value." >&2
                exit 1
            fi
            IDENTITY="$2"
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
        --build-type)
            if [[ $# -lt 2 ]]; then
                echo "--build-type requires a value." >&2
                exit 1
            fi
            BUILD_TYPE="$2"
            shift 2
            ;;
        --skip-build)
            SKIP_BUILD=1
            shift
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

if ! command -v codesign >/dev/null 2>&1; then
    echo "codesign is required but was not found in PATH." >&2
    exit 1
fi

if ! xcode-select -p >/dev/null 2>&1; then
    echo "Xcode Command Line Tools are required on macOS." >&2
    exit 1
fi

resolve_identity() {
    local -a identities

    while IFS= read -r identity; do
        identities+=("${identity}")
    done < <(
        security find-identity -v -p codesigning \
            | sed -n 's/.*"\(Developer ID Application:.*\)".*/\1/p'
    )

    if [[ ${#identities[@]} -eq 0 ]]; then
        echo "No valid Developer ID Application identities were found." >&2
        echo "Create or repair the certificate first with:" >&2
        echo "  security find-identity -v -p codesigning" >&2
        exit 1
    fi

    if [[ ${#identities[@]} -gt 1 ]]; then
        echo "Multiple valid Developer ID Application identities were found." >&2
        echo "Pass --identity with the exact one you want to use:" >&2
        printf '  %s\n' "${identities[@]}" >&2
        exit 1
    fi

    IDENTITY="${identities[0]}"
}

if [[ -z "${IDENTITY}" ]]; then
    resolve_identity
fi

if [[ "${SKIP_BUILD}" -ne 1 ]]; then
    BUILD_TYPE="${BUILD_TYPE}" "${ROOT_DIR}/rebuild.sh"
fi

if [[ ! -d "${APP_PATH}" ]]; then
    echo "App bundle not found: ${APP_PATH}" >&2
    exit 1
fi

echo "Signing: ${APP_PATH}"
echo "Identity: ${IDENTITY}"

codesign --force \
    --options runtime \
    --timestamp \
    --sign "${IDENTITY}" \
    "${APP_PATH}"

echo
echo "Verifying signature..."
codesign --verify --deep --strict --verbose=2 "${APP_PATH}"

echo
echo "Signature details:"
codesign -dv --verbose=4 "${APP_PATH}" 2>&1
