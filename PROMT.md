# Port XUI to another client

Give the agent this repository, the target repository and the reference
screenshots, then use the prompt below.

---

Port **XUI (xxx client)** into the target client. Match the original interface
and include all of its menus, widgets, materials and transitions. The scope is
the UI: connect it to the target client's existing data and actions rather than
porting xxx client features or replacing the target's game implementation.

## Read the source

Read both repositories' instructions and build files first. Inspect:

- `src/game/client/xui/xui.h`, `theme.h` and `layout.cpp`;
- `src/game/client/xui/widgets/` and `screens/`;
- `src/game/client/components/menu_services/` to understand screen data contracts;
- `IGraphics::SUiRect`, backdrop capture and `data/shader/vulkan/` for UI rendering;
- `LICENSE`, `licenses/README.md` and the relevant font/icon licenses.

Check the actual engine APIs and C++ standard before adapting code. Preserve
existing work in the target repository. Do not overwrite complete core files.

## Interface to reproduce

Use the source and actual screenshots as the reference. Keep the composition,
spacing, typography, colors, icons, radii and interaction states consistent.

- Home screen, background, server cards, friends, profile entry and bottom dock.
- Server browser, communities, filters, search, details and context menus.
- Settings navigation, search, controls, dependent rows and theme customization.
- Own and viewed player profiles, map history and independent scrolling regions.
- In-game menu, scoreboard, chat, voting and demo navigation.
- HUD editor, draggable modules, movable/hideable controls and alignment guides.
- Dynamic Island presentation, previews and transitions.
- Popups, dialogs, notifications, tooltips, loading and empty/error states.
- About screen: client icon, version, GitHub link pills, credits and licenses.
- Resource catalog screens and native background-file selection UI.

Inspect the screen directory for additional routes and states. Do not stop at
recreating only the home screen or overlaying a new skin on old menu layouts.
Preserve the target client's own name, version and authors in its About page,
and identify XUI separately as the interface source.

## Integration

Keep the target's networking, physics, prediction, game renderer, demos and
editor infrastructure. Use small adapters for the data and callbacks that XUI
needs. Reuse target services rather than copying xxx backend or gameplay logic.

Map each visible control to an existing target action or setting. If the target
has no equivalent capability, omit the corresponding control and report that
integration gap. Do not add the underlying feature just to fill a source screen,
and do not leave dead buttons or invented data.

Keep menu rendering separate from data providers and filesystem/network work.
Add new files to the target build lists and retain the target's code conventions.

## Rendering and behavior

Preserve SDF edges, gradients, shadows, real backdrop blur, subtle refraction,
clipping, batching and alpha. If the target renderer differs, implement the UI
rendering adapter it needs. A flat translucent panel does not replace glass.
Capture the backdrop before drawing panel contents so text remains sharp.

Use the existing layout engine and widgets, with stable IDs for focus, fields,
tabs, scroll regions and dragging. Do not replace layouts with independent fixed
coordinates. Match the reference at 1440×900 and test narrower windows, resizing,
HiDPI and larger UI scales.

Use frame delta and persistent animation state. Apply the selected accent on
all screens, including connection and map-loading indicators. Both themes must
keep menu text and HUD previews readable. Scroll input must work over text and
empty container space; check nested areas, boundaries and scrollbars.

Support mouse and keyboard navigation, Escape/Enter, language switching and
transitions among menus, gameplay, demos and the editor. Preserve existing
English and Russian UI translations. Keep the HUD editor controls movable and
hideable so every part of the canvas remains accessible.

## Attribution

Preserve applicable third-party notices. Retain the terms for the original XUI
design from Part II of `LICENSE` when distributing covered design materials.
Add a visible credit in the repository README or in-game About/Credits page:

> Interface design based on XUI (xxx client), by IMDelewer — https://github.com/IMDelewer.

Credit the target's own authors separately. Reimplementing the protected design
with different code or from screenshots does not remove its attribution terms.

## Verification

Build the target and fix errors. With owner authorization, run it with isolated
settings. Compare real screenshots against the source and check every menu,
both themes, accent changes, long profiles, scrolling, HUD editor movement and
hiding, popups, keyboard input and window resizing.

Deliver the integrated UI, required rendering adapters, license notices and
build changes. Summarize completed screens, checks and any unavailable target
capabilities. Do not claim a screen or platform was tested unless it was run.
