# Building xxx client

[English](BUILDING.md) · [Русский](ru/BUILDING.md)

Use CMake, Python 3, stable Rust/Cargo and a C++20 compiler. Build outside the
source tree. The client requires Vulkan; `SERVER`, `TOOLS`, `AUTOUPDATE` and
`DDEV_XX` are off by default. `VIDEORECORDER=OFF` avoids FFmpeg; demos still work.

## Linux — Ubuntu 24.04

```sh
sudo apt-get update
sudo apt-get install build-essential cmake ninja-build pkg-config python3 cargo rustc \
  libcurl4-openssl-dev libfreetype6-dev libogg-dev libopus-dev libopusfile-dev \
  libpng-dev libsdl2-dev libsqlite3-dev libssl-dev libwavpack-dev zlib1g-dev \
  libvulkan-dev glslang-tools spirv-tools libnotify-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCLIENT=ON -DSERVER=OFF -DTOOLS=OFF -DVULKAN=ON \
  -DVIDEORECORDER=OFF -DDDEV_XX=OFF -DINFORM_UPDATE=OFF
cmake --build build --target game-client --parallel
./build/xxx
```

If your distribution's Rust is older than `Cargo.toml` requires, use the stable
rustup toolchain. Install `zenity` or `kdialog` for the PNG file picker.

## macOS

```sh
brew install cmake ninja pkg-config rust python vulkan-headers glslang spirv-tools dylibbundler
python3 scripts/fetch_macos_dependencies.py --output .cache/runtime
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCLIENT=ON -DSERVER=OFF -DTOOLS=OFF -DVULKAN=ON \
  -DVIDEORECORDER=OFF -DDDEV_XX=OFF -DINFORM_UPDATE=OFF -DIPO=ON \
  -DPREFER_BUNDLED_LIBS=ON -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DDDNET_LIBS_DIR="$PWD/.cache/runtime/ddnet-libs" \
  -DVULKAN_LIBRARIES="$PWD/.cache/runtime/MoltenVK/libMoltenVK.dylib" \
  -DVULKAN_INCLUDE_DIRS="$(brew --prefix vulkan-headers)/include"
cmake --build build --target game-client --parallel
```

The runtime libraries are pinned independently of Homebrew. Packaging checks every
Mach-O file against the macOS 12 target, includes only the target architecture,
verifies the ad-hoc signature and creates a compressed DMG. This checks binary
requirements; it does not replace testing on an actual macOS 12 installation.

## Windows x64

Install Visual Studio 2022 with Desktop development with C++, CMake, Python 3,
stable Rust for MSVC, and the Vulkan SDK. From the source root:

```powershell
git clone https://github.com/ddnet/ddnet-libs ddnet-libs
git -C ddnet-libs checkout c0e6703fbcdbe03df2f26875427ec3951ce4ec21
cmake -S . -B build -A x64 -DCLIENT=ON -DSERVER=OFF -DTOOLS=OFF `
  -DVULKAN=ON -DVIDEORECORDER=OFF -DDDEV_XX=OFF -DINFORM_UPDATE=OFF
cmake --build build --target game-client --config Release --parallel
```

The executable is `build/Release/xxx.exe` or `build/xxx.exe`, depending on the
generator. CI uses the same pinned revision of ddnet-libs.

## Packaging

```sh
python3 scripts/check_public_tree.py --binary build/xxx
python3 scripts/package_client.py --build build --output dist
```

On Windows, pass `build/Release/xxx.exe` to the check; the packager finds it
automatically. On macOS, install `dylibbundler` with Homebrew first. Packages
include runtime assets, a short user guide and licenses. Developer documentation,
screenshots and optional language fonts stay out of the main package. macOS signing is ad-hoc;
Developer ID signing and notarization are not configured.

The Linux archive depends on Ubuntu 24.04 system libraries; it is not an AppImage.
Package reports record installed and compressed sizes. The default release
budget is 45 MiB. Run `python3 -m unittest discover -s scripts -p test_packaging.py`
to check the packaging rules and version-specific release notes.

## Developer build

```sh
cmake -S . -B build-dev -G Ninja -DCMAKE_BUILD_TYPE=Debug \
  -DCLIENT=ON -DSERVER=OFF -DTOOLS=OFF -DVULKAN=ON \
  -DVIDEORECORDER=OFF -DDDEV_XX=ON -DINFORM_UPDATE=OFF
cmake --build build-dev --target game-client --parallel
```

`xx_after`, `xx_screenshot`, `xx_dev_*`, test notifications and the widget gallery
are excluded from normal builds. F10 and the regular `screenshot` command are
available in normal builds. Use a separate storage directory for visual testing.

## Core C++ tests

Install GTest, then use a separate configuration. Upstream tests require the
server objects; do not use this configuration to package the client.

```sh
cmake -S . -B build-tests -DCMAKE_BUILD_TYPE=Release \
  -DCLIENT=OFF -DSERVER=ON -DTOOLS=OFF -DDOWNLOAD_GTEST=OFF
cmake --build build-tests --target run_cxx_tests --parallel
```

The local run passed 371 tests; 3 are disabled upstream. The client was built
and launched on macOS 26.7.2, Apple M4, using Vulkan through MoltenVK.

If your network requires a proxy, libcurl may need `https_proxy` in the client
process environment. macOS system proxy settings are not picked up automatically.
No proxy address is hardcoded in the client.

## GitHub Releases

Run the **Release** workflow manually, or push a `v` tag matching `XXX_VERSION`.
It reuses the desktop build matrix, checks the package count and version, writes
`SHA256SUMS`, and publishes with `softprops/action-gh-release`. Release descriptions come from the current entry in `CHANGELOG.md`. No local GitHub CLI
or manually maintained `dist/` directory is required. The repository must be
uploaded to GitHub with Actions enabled before this workflow can run.
