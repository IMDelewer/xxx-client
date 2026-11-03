# xxx client

Public DDNet fork with XUI. Reply to the owner in Russian. Keep source code
and primary documentation in English, with Russian documentation in `docs/ru/`.

- Use XUI for menus. Menu services live in `components/menu_services/`.
- Do not add cheats, spoofing, input automation, trajectories or gameplay aids.
  Diagnostics belong behind `DDEV_XX`, which defaults to OFF.
- Preserve DDNet physics, protocol, server, demos and editor infrastructure.
- Read files before editing. Follow DDNet naming and tab indentation. Add new
  source files to the explicit alphabetical CMake lists.
- Format only changed C++ files. Preserve unrelated edits and secrets.
- Do not commit or publish without the owner's instruction.
- Keep the original-code and XUI design terms in `LICENSE`; retain upstream
  notices in `licenses/`. Credit XUI (xxx client) when porting the design.
- Build out of tree: `cmake --build build --target game-client`.
- Before packaging: `python3 scripts/check_public_tree.py --binary build/xxx`.
- Launch the client only with the owner's permission. Use isolated settings
  and actual renderer screenshots for visual checks.
