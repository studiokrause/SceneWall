# SceneWall

**SceneWall brings the vMix multiview idea to OBS Studio** — a dockable panel where every scene becomes a live thumbnail you can switch with a single click. It started as a port of vMix's source list, but it is not a clone: SceneWall adds features vMix's source list does not offer, such as fully user-defined tab groups with custom colors, per-scene headers, collapsible scene bars, a wrapping (flex-like) layout, one-click *Autosize*, drag & drop reordering, and full localization.

## What it is

SceneWall is a dock panel for OBS Studio. It shows the scenes of the current scene collection as live thumbnails, arranged in a wrapping grid that reflows to the width of the dock. It is meant as a fast, visual switcher for operators who work with many scenes (cameras, overlays, helpers) and want to see them all at once — similar to a video switcher's multiview.

Unlike vMix's source list, SceneWall lets you organize scenes into **named tab groups with their own colors**, rename the special **All** tab, collapse individual scenes into compact vertical bars, reorder tabs and scenes by dragging, and resize everything with one slider or a single *Autosize* click.

## Features

- **Live scene thumbnails** rendered through the OBS graphics subsystem, refreshed continuously.
- **Wrapping layout** — thumbnails (and collapsed bars) reflow like a flex container when the dock is resized.
- **Autosize** — computes the largest thumbnail size that still fits every scene of the current tab in the visible dock area, then applies that size to **all** tabs. Collapsed bars are counted as narrower than full thumbnails, so the packing stays efficient.
- **Realtime mode** — refreshes thumbnails at the frame rate OBS is currently running at (`obs_get_active_fps`). Because this can noticeably increase CPU/GPU usage, OBS asks for confirmation before it is enabled.
- **Optimized thumbnail rendering** — GPU scratch buffers (`gs_texrender` / `gs_stagesurf`) are allocated once per thumbnail and reused instead of being recreated every frame, and any thumbnail that is not actually on screen (hidden dock, another tab, scrolled out of view) is skipped entirely.
- **Tab groups with colors** — create, rename, recolor and remove tabs. Each tab's color is used for its scene headers, for the headers of the scenes assigned to it, and for the tab button itself.
- **Scene assignment** — assign any scene to a tab. Assignments are stored against a **stable tab ID**, so renaming or reordering a tab never loses its scenes.
- **Drag & drop reordering of tabs** — drag a tab button to a new position; the order is saved between OBS sessions.
- **Drag & drop reordering of scenes** — drag a thumbnail inside a tab to reorder it. **Each tab keeps its own independent order, and the order is completely independent of the scene list in OBS.** Saved between sessions.
- **Scene headers** — every scene has a header with white text on a configurable background color. Right-click the header to collapse the scene into a slim vertical bar; right-click again to expand it.
- **Program / Preview indicators** — red frame for the current Program scene, green frame for the current Preview scene in Studio Mode.
- **Localization** — English, German, Polish, Ukrainian, Italian, Spanish and French, following OBS's interface language. The default title of the **All** tab is translated too.

## Using SceneWall

### Mouse and keyboard

- **Left click** on a thumbnail — switch to that scene (set it as the current Program scene).
- **Left drag** on a thumbnail — reorder it inside the current tab.
- **Right click (PPM)** on a scene header — collapse that scene into a vertical bar (or expand it again when already collapsed).
- **Right click (PPM)** on a thumbnail — in **Studio Mode** sets the scene as the Preview scene. A plain right-click never opens a menu.
- **CTRL + click** on a thumbnail — opens the **plugin menu**:
  - *Assign to Tab* — put the scene into one of your tab groups.
  - *Remove from Tab* — return the scene to **All** only.
  - *Collapse* / *Expand* — collapse the scene into a vertical bar or expand it back.
  - *Properties* — open the scene's properties.

### Autosize

Click **Autosize** in the toolbar to let SceneWall pick the largest thumbnail size that fits every scene of the current tab inside the visible dock area. The size is then applied to **all tabs**, so the panel stays consistent. SceneWall re-flows the layout afterwards, so thumbnails wrap into rows instead of stacking into a single column. With a very large number of scenes the size stops at the slider's minimum (80 px) and the panel scrolls vertically.

### Realtime

The **Realtime** toggle switches thumbnails from the default 2 fps refresh to the frame rate OBS is currently using. Because this can noticeably increase CPU/GPU usage, OBS shows a confirmation warning the first time you enable it. Leave it off for everyday use; turn it on when you need near-instant visual feedback.

### Tabs and scene assignment

- The **All** tab always shows every scene. Its title can be renamed like any other tab, and on first run it uses the current OBS interface language. It cannot be deleted.
- Create, rename, recolor and remove tabs in **Settings** (the gear button). Clicking the color cell opens a color picker; the hex value is only shown inside the picker.
- To put a scene in a group, **CTRL + click** it and choose *Assign to Tab*. A scene belongs to one tab; unassigned scenes appear only in **All**.
- Assignments are keyed by a **stable tab ID**, so changing a tab's name keeps all of its scenes. Removing a tab returns its scenes to **All**.
- Reorder tabs by dragging them in the tab bar. Reorder scenes by dragging thumbnails inside a tab. Both orders — and both are **independent per tab and independent of OBS's own scene list** — are saved between sessions.

### Scene headers and bars

Each thumbnail has a header showing the scene name in white. The header uses the color of the tab the scene is **assigned** to, so even while browsing **All** you can see at a glance which group a scene belongs to. Right-clicking the header collapses the scene into a slim vertical bar (with the name rotated); right-clicking again expands it. This is handy for keeping rarely used scenes around without wasting space.

## Installation

1. Copy `SceneWall.dll` to `OBS_FOLDER/obs-plugins/64bit/`.
2. Copy the contents of `data/obs-plugins/SceneWall/` (the `locale` folder) to `OBS_FOLDER/data/obs-plugins/SceneWall/`.
3. Restart OBS Studio.

SceneWall uses only libraries shipped with OBS (`obs.dll`, `obs-frontend-api.dll`, Qt6).

## Building from source

Requirements:

- Windows 10/11 x64, Visual Studio 2019/2022 (MSVC, x64).
- CMake 3.16+.
- An installed OBS SDK (headers + import libraries).
- A Qt6 build (CMake package), e.g. the `obs-deps` Qt6 package.

The paths to the OBS SDK and Qt6 are set at the top of `CMakeLists.txt` (`OBS_SDK_DIR` and the `CMAKE_PREFIX_PATH` entry). Adjust them to your machine, then:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The resulting `build/Release/SceneWall.dll` is the plugin.

## Localization

All user-facing strings go through OBS's translation mechanism (`obs_module_text`). Translation files live in `data/obs-plugins/SceneWall/locale/*.ini` and follow the OBS interface language. Currently included: **en-US, de-DE, pl-PL, uk-UA, it-IT, es-ES, fr-FR**. Contributions for more languages are welcome.

## Versioning

The current version is **1.1**. The version is shown in the plugin's *About* dialog.

## License

SceneWall is **free and open-source software (FOSS)**, released under the **MIT License**. You are free to use, study, modify and redistribute it, including commercially, as long as the copyright notice and license text are preserved.

## Credits

- Author: **Studio Krause**
- Developed with: **Tencent Hy4 Preview**
- Companion plugin: [studio-no-transition-strip](https://github.com/studiokrause/studio-no-transition-strip).
