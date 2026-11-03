# Сборка xxx client

[English](../BUILDING.md) · [Русский](BUILDING.md)

Нужны CMake, Python 3, Rust/Cargo stable и компилятор C++20. Собирайте вне
исходников. Основная инструкция — в [README](README.md).

### Linux — Ubuntu 24.04

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

Если установленный Rust старее указанного в `Cargo.toml`, используйте актуальный
stable toolchain через rustup. Для выбора PNG через системный диалог установите
`zenity` или `kdialog`; сам клиент не требует их для запуска.

### macOS

```sh
brew install cmake ninja pkg-config rust python sdl2 freetype libpng \
  opusfile wavpack openssl molten-vk vulkan-headers glslang spirv-tools
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCLIENT=ON -DSERVER=OFF -DTOOLS=OFF -DVULKAN=ON \
  -DVIDEORECORDER=OFF -DDDEV_XX=OFF -DINFORM_UPDATE=OFF
cmake --build build --target game-client --parallel
./build/xxx
```

Архив с `.app` и библиотеками:

```sh
brew install dylibbundler
python3 scripts/package_client.py --build build --output dist
```

Пакет имеет локальную ad-hoc подпись. Developer ID и Apple notarization здесь
не настроены.

### Windows x64

Установите Visual Studio 2022 с C++ Desktop Development, CMake, Python 3,
Rust stable MSVC и Vulkan SDK. Из корня исходников:

```powershell
git clone https://github.com/ddnet/ddnet-libs ddnet-libs
git -C ddnet-libs checkout c0e6703fbcdbe03df2f26875427ec3951ce4ec21
cmake -S . -B build -A x64 -DCLIENT=ON -DSERVER=OFF -DTOOLS=OFF `
  -DVULKAN=ON -DVIDEORECORDER=OFF -DDDEV_XX=OFF -DINFORM_UPDATE=OFF
cmake --build build --target game-client --config Release --parallel
python scripts/package_client.py --build build --output dist
```

Команды выше получают внешние библиотеки DDNet, а не читы. CI использует тот же
зафиксированный revision библиотек. Готовый `.exe` находится в `build/Release/`
или `build/`, в зависимости от генератора CMake.

## Разработка

Обычная сборка использует `DDEV_XX=OFF`. В ней нет команд `xx_after`,
`xx_screenshot`, `xx_dev_*`, тестовых уведомлений и галереи виджетов.
Обычная пользовательская команда `screenshot` и F10 сохранены.

Для диагностики интерфейса:

```sh
cmake -S . -B build-dev -G Ninja -DCMAKE_BUILD_TYPE=Debug -DDDEV_XX=ON \
  -DVULKAN=ON -DCLIENT=ON -DSERVER=OFF -DTOOLS=OFF -DVIDEORECORDER=OFF
cmake --build build-dev --target game-client --parallel
```

Настройки, исходники и проверки: [архитектура](ARCHITECTURE.md),
[перенос XUI](../../PROMT.md).
Перед упаковкой release-сборки выполните:

```sh
python3 scripts/check_public_tree.py --binary build/xxx
```


## Штатные C++-тесты DDNet

Нужен GTest и отдельная конфигурация с сервером:

```sh
cmake -S . -B build-tests -DCMAKE_BUILD_TYPE=Release \
  -DCLIENT=OFF -DSERVER=ON -DTOOLS=OFF -DDOWNLOAD_GTEST=OFF
cmake --build build-tests --target run_cxx_tests --parallel
```

`SERVER=ON` требуется upstream-тестам. Для упаковки используйте клиентскую
Release-сборку с `DDEV_XX=OFF`. Локально прошёл 371 тест; 3 отключены в upstream.

На macOS libcurl может потребовать переменную `https_proxy`, если ресурсы
доступны только через ваш прокси: системная настройка macOS автоматически
не подхватывается. Адрес прокси в клиенте не задан.

## GitHub Releases

Запустите workflow **Release** вручную или отправьте тег `v` с версией из
`XXX_VERSION`. Он использует матрицу desktop-сборок, проверяет версию и архивы,
создаёт `SHA256SUMS` и публикует релиз через `softprops/action-gh-release`.
Локальный GitHub CLI и готовый каталог `dist/` для этого не нужны. Сначала
нужно загрузить репозиторий на GitHub и включить Actions.
