#!/bin/bash
set -e

if [ -z ${1+x} ]; then
        DIR="$HOME"
else
        DIR="${1}"
fi

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
# shellcheck source=scripts/compile_libs/_build_common.sh
source "${SCRIPT_DIR}/../compile_libs/_build_common.sh"

SDK_DIR="${DIR}/Android/Sdk"
mkdir -p "${SDK_DIR}"
cd "${SDK_DIR}"

# Returns the single top-level directory inside "$1" (how Google zips are structured).
inner_dir_of() {
        find "$1" -mindepth 1 -maxdepth 1 -type d | head -n1
}

# Downloads and extracts a Google SDK zip into a versioned subdirectory of the current directory.
# Skips the download if the target directory already exists (idempotent, cache-friendly).
# Usage: download_component <url> <filename> <target_dir>
download_component() {
        URL="$1"
        FILENAME="$2"
        TARGET="$3"
        if [ -d "${TARGET}" ]; then
                log_info "${TARGET} already present, skipping download"
                return 0
        fi
        mkdir -p "${TARGET}.tmp.dl"
        wget --quiet "${URL}" -O "${TARGET}.tmp.dl/${FILENAME}"
        TMP_EXTRACT="${TARGET}.tmp.dl/extract"
        mkdir -p "${TMP_EXTRACT}"
        unzip -q "${TARGET}.tmp.dl/${FILENAME}" -d "${TMP_EXTRACT}"
        INNER="$(inner_dir_of "${TMP_EXTRACT}")"
        if [ -z "${INNER}" ]; then
                log_error "ERROR: zip ${FILENAME} does not contain a top-level directory"
                rm -rf "${TARGET}.tmp.dl"
                exit 1
        fi
        mv "${INNER}" "${TARGET}"
        rm -rf "${TARGET}.tmp.dl"
}

# Android NDK (needed by CMake and cargo-ndk)
(
        mkdir -p ndk
        cd ndk
        log_info "Downloading Android NDK to ${PWD}"
        download_component \
                "https://dl.google.com/android/repository/android-ndk-r29-${HOST_OS}.zip" \
                "android-ndk-r29-${HOST_OS}.zip" \
                "29.0.14206865"
)

# Build tools (aapt2, zipalign, apksigner)
(
        mkdir -p build-tools
        cd build-tools
        log_info "Downloading Android build tools to ${PWD}"
        download_component \
                "https://dl.google.com/android/repository/build-tools_r36.1_${HOST_OS}.zip" \
                "build-tools_r36.1_${HOST_OS}.zip" \
                "36.1.0"
)

# Command line tools (sdkmanager for licenses and platform install)
(
        mkdir -p cmdline-tools
        cd cmdline-tools
        log_info "Downloading Android command-line tools to ${PWD}"
        download_component \
                "https://dl.google.com/android/repository/commandlinetools-${HOST_OS}-13114758_latest.zip" \
                "commandlinetools-${HOST_OS}-13114758_latest.zip" \
                "latest"
        # The commandlinetools zip ships a top-level "cmdline-tools" directory, so the
        # naive extract produces latest/cmdline-tools/bin/sdkmanager while the SDK
        # layout requires latest/bin/sdkmanager. Hoist the contents one level up.
        if [ ! -f latest/bin/sdkmanager ] && [ -f latest/cmdline-tools/bin/sdkmanager ]; then
                log_info "Fixing nested cmdline-tools layout"
                mv latest/cmdline-tools/* latest/
                rmdir latest/cmdline-tools
        fi
)

# Accept all SDK licenses and pre-install the platform the project builds against,
# so Gradle/AGP never has to download anything mid-build.
# NOTE: the sdkmanager lives under cmdline-tools/latest/bin relative to SDK_DIR,
# so use the full absolute path (cwd here is SDK_DIR, NOT cmdline-tools).
CMDLINE_TOOLS_BIN="${SDK_DIR}/cmdline-tools/latest/bin"
if [ -f "${CMDLINE_TOOLS_BIN}/sdkmanager" ]; then
        log_info "Accepting SDK licenses"
        yes 2> /dev/null | "${CMDLINE_TOOLS_BIN}/sdkmanager" --sdk_root="${SDK_DIR}" --licenses > /dev/null
        log_info "Installing platforms;android-36"
        yes 2> /dev/null | "${CMDLINE_TOOLS_BIN}/sdkmanager" --sdk_root="${SDK_DIR}" "platforms;android-36" > /dev/null
else
        log_error "ERROR: sdkmanager not found at ${SDK_DIR}/cmdline-tools/latest/bin/sdkmanager"
        exit 1
fi

log_info "Android SDK setup complete: ${SDK_DIR}"
