# Contributing

[English](CONTRIBUTING.md) · [Русский](ru/CONTRIBUTING.md)

See [Building](BUILDING.md) and [Source layout](architecture/README.md).

- Use XUI for menus and widgets. Keep HTTP and filesystem services separate
  from screens. Use colors and dimensions from `xui/theme.h`.
- Preserve DDNet physics, protocol, map and demo formats.
- Do not add cheats, trajectories, spoofing, fake ping or automatic input
  correction. Diagnostics belong behind `DDEV_XX`.
- Add translations and persistence for settings, and hide dependent controls
  when their parent is disabled. Network requests must not block a frame.
- Run `clang-format` on changed C++ files. Update the explicit CMake source list
  when adding or moving files. Follow DDNet naming and tab indentation.

Describe the problem, resulting behavior and checks in a pull request. Include
real client screenshots for visual changes and state any untested platforms.
For performance comparisons, keep the map, camera, resolution, UI scale and
FPS limit identical; measure glass and blur separately. The dev-only command
`xx_dev_perf <label> <seconds>` records frame times.

Preserve source attribution and license notices. The [XUI porting prompt](../PROMT.md)
describes the interface, integration requirements and design attribution.
