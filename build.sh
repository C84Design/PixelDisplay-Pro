#!/usr/bin/env bash
#
# PixelDisplay Pro — one-shot build helper.
#
# Builds the After Effects plugin (PixelDisplayPro.plugin on macOS,
# PixelDisplayPro.aex on Windows) plus the engine test suite.
#
# Usage:
#   ./build.sh /path/to/AfterEffectsSDK        # explicit SDK root
#   PD_AE_SDK_ROOT=/path/to/SDK ./build.sh     # or via env var
#   ./build.sh                                 # tries common default locations
#
# After a successful build the plugin bundle path is printed. Copy it into
# your After Effects "Plug-ins" folder and restart AE; it appears under the
# "PixelDisplay" category in the Effects menu.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${REPO_ROOT}/build"
CONFIG="${PD_BUILD_CONFIG:-Release}"

# --- resolve the SDK root -------------------------------------------------
SDK_ROOT="${1:-${PD_AE_SDK_ROOT:-}}"

if [[ -z "${SDK_ROOT}" ]]; then
    for candidate in \
        "${HOME}/AfterEffectsSDK" \
        "${HOME}"/AfterEffectsSDK_* \
        "${HOME}/Downloads/AfterEffectsSDK" \
        "${HOME}/Downloads"/AfterEffectsSDK_* ; do
        if [[ -d "${candidate}" ]]; then SDK_ROOT="${candidate}"; break; fi
    done
fi

if [[ -z "${SDK_ROOT}" ]]; then
    echo "ERROR: no Adobe After Effects SDK found." >&2
    echo "Pass its path:  ./build.sh /path/to/AfterEffectsSDK" >&2
    exit 1
fi

# The build expects <SDK_ROOT>/Examples/Headers/AE_Effect.h. If the caller
# pointed one level too high or too low, try to correct automatically.
if [[ ! -f "${SDK_ROOT}/Examples/Headers/AE_Effect.h" ]]; then
    if [[ -f "${SDK_ROOT}/Headers/AE_Effect.h" ]]; then
        # pointed at the Examples dir itself
        SDK_ROOT="$(cd "${SDK_ROOT}/.." && pwd)"
    else
        found="$(find "${SDK_ROOT}" -maxdepth 4 -name AE_Effect.h -path '*/Examples/Headers/*' 2>/dev/null | head -1 || true)"
        if [[ -n "${found}" ]]; then
            SDK_ROOT="$(cd "$(dirname "${found}")/../.." && pwd)"
        fi
    fi
fi

if [[ ! -f "${SDK_ROOT}/Examples/Headers/AE_Effect.h" ]]; then
    echo "ERROR: '${SDK_ROOT}' does not look like an AE SDK" >&2
    echo "       (expected Examples/Headers/AE_Effect.h beneath it)." >&2
    exit 1
fi

echo "==> AE SDK      : ${SDK_ROOT}"
echo "==> Build type  : ${CONFIG}"
echo "==> Build dir   : ${BUILD_DIR}"
echo

# --- configure + build ----------------------------------------------------
cmake -S "${REPO_ROOT}/PixelDisplay" -B "${BUILD_DIR}" \
      -DCMAKE_BUILD_TYPE="${CONFIG}" \
      -DPD_AE_SDK_ROOT="${SDK_ROOT}"

cmake --build "${BUILD_DIR}" --config "${CONFIG}" -j

echo
echo "==> Build succeeded."

# --- report the artifact + run tests -------------------------------------
bundle="$(find "${BUILD_DIR}" -maxdepth 4 \( -name 'PixelDisplayPro.plugin' -o -name 'PixelDisplayPro.aex' \) 2>/dev/null | head -1 || true)"
if [[ -n "${bundle}" ]]; then
    echo "==> Plugin      : ${bundle}"
    echo "    Copy it into your After Effects 'Plug-ins' folder and restart AE."
fi

tests="$(find "${BUILD_DIR}" -maxdepth 3 -name pdtests -type f 2>/dev/null | head -1 || true)"
if [[ -n "${tests}" ]]; then
    echo
    echo "==> Running engine tests..."
    "${tests}"
fi
