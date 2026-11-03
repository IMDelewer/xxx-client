# Source layout

[English](README.md) · [Русский](../ru/ARCHITECTURE.md)

| Path | Responsibility |
| --- | --- |
| `src/game/client/xui/` | Immediate-mode layout, input, themes, widgets and screens |
| `src/game/client/components/menu_services/` | Browser data, resources, demos, teedata, native PNG selection and map previews |
| `src/game/client/components/xxx/` | Cosmetic components, Dynamic Island and menu/game transitions |
| `src/game/client/dev/`, `src/game/client/xui/dev/` | Diagnostics behind `DDEV_XX` |
| `src/engine/shared/config_variables_xxx.h` | xxx configuration variables |
| `src/engine/client/backend/vulkan/`, `data/shader/vulkan/` | Vulkan backend and XUI GPU materials |
| `src/game/client/prediction/` | Ordinary DDNet client prediction |
| `data/languages/xxx/` | Additional translations |
| `scripts/package_client.py` | Runtime package, assets and license notices |
| `.github/workflows/build.yml` | Builds for Windows, Linux and macOS |

Keep menu rendering in XUI and networking/filesystem operations in services.
Reuse the DDNet game renderer, physics, protocol, demos and editor infrastructure.
`map_previews` provides menu artwork and map overviews; it has no in-game radar
or finish-progress calculation. Server sources are retained for local servers
and upstream tests; `SERVER` is off by default.

A new setting needs its control, translation, persistence and dependent-control
visibility. Register new source files in the alphabetical CMake lists.
Diagnostic commands and screens must stay behind `DDEV_XX`, which defaults to off.
