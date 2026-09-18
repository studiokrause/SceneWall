# SceneWall

**SceneWall brings the vMix multiview idea to OBS Studio** — a dockable panel where every scene becomes a live thumbnail you can switch with a single click. It started as a port of vMix's source list, but it is not a clone: SceneWall adds features vMix's source list does not offer, such as fully user-defined tab groups with custom colors, per-scene headers, collapsible scene bars, a wrapping (flex-like) layout, one-click *Autosize*, drag & drop reordering, and full localization.

![SceneWall docked in OBS Studio](scenewall_screenshot.png)

> **Note about the screenshot:** the middle toolbar that normally sits between the *Preview* and *Program* views is not visible here, because the author runs the companion plugin **[studio-no-transition-strip](https://github.com/studiokrause/studio-no-transition-strip)**, which moves OBS's transition strip (Transition button, T-bar and quick transitions) into a movable dock.

## What it is

A dock panel that shows the scenes of the current scene collection as live thumbnails in a wrapping grid that reflows to the width of the dock. Built for operators who work with many scenes (cameras, overlays, helpers) and want to see them all at once — similar to a video switcher's multiview.

Unlike vMix's source list, SceneWall lets you organize scenes into **named tab groups with their own colors**, rename the special **All** tab, collapse individual scenes into compact vertical bars, reorder tabs and scenes by dragging, and resize everything with one slider or a single *Autosize* click.

## Features

### Thumbnails and layout

- **Live scene thumbnails** rendered through the OBS graphics subsystem.
- **Wrapping layout** — thumbnails and collapsed bars reflow like a flex container when the dock is resized.
- **Autosize** — computes the largest size that fits every scene of the current tab in the visible area, then applies it to all tabs. Collapsed bars count as narrower, so packing stays efficient.
- **Realtime mode** — refreshes at the frame rate OBS is currently running at, after a resource-usage confirmation.
- **Optimized rendering** — GPU scratch buffers are allocated once per thumbnail and reused, and anything not actually visible (hidden dock, other tab, scrolled out of view) is skipped entirely.

### Tabs and scenes

- **Tab groups with colors** used for the tab buttons and for the headers of the scenes assigned to them.
- **Scene assignment** keyed by a **stable tab ID**, so renaming or reordering a tab never loses its scenes.
- **Drag & drop reordering of tabs** — drag a tab button to a new position; the order is saved between sessions.
- **Drag & drop reordering of scenes** — drag a thumbnail inside a tab to reorder it. Each tab keeps its own order, completely independent of the scene list in OBS. Saved between sessions.

### Scene headers and indicators

- **Scene headers** with white text on a configurable background color. Right-click a header to collapse the scene into a slim vertical bar; right-click again to expand it.
- **Program (red) and Preview (green) indicators** — red frame for the current Program scene, green frame for the current Preview scene in Studio Mode.
- **Localization** — English, German, Polish, Ukrainian, Italian, Spanish and French. The default title of the **All** tab is translated too.

## Mouse and keyboard

| Action | Result |
| --- | --- |
| **Left click** on a thumbnail | Switch to that scene |
| **Left drag** on a thumbnail | Reorder it inside the current tab |
| **Right click** on a scene header | Collapse / expand that scene |
| **Right click** on a thumbnail | In Studio Mode, set it as the Preview scene. A plain right-click never opens a menu |
| **CTRL + click** on a thumbnail | Plugin menu: *Assign to Tab*, *Remove from Tab*, *Collapse / Expand*, *Properties* |

## Tabs and assignment

- The **All** tab always shows every scene. Its title can be renamed and it cannot be deleted. On first run it uses the OBS interface language.
- Create, rename, recolor and remove tabs in **Settings** (the gear button). Clicking a color cell opens a color picker.
- To put a scene in a group, **CTRL + click** it and choose *Assign to Tab*. A scene belongs to one tab; unassigned scenes appear only in **All**.
- A scene's header takes the color of the tab it is **assigned** to, so its group stays recognisable even while browsing **All**.

## Installation

1. Copy `SceneWall.dll` to `OBS_FOLDER/obs-plugins/64bit/`
2. Copy the `locale` folder to `OBS_FOLDER/data/obs-plugins/SceneWall/`
3. Restart OBS Studio

SceneWall uses only libraries shipped with OBS (`obs.dll`, `obs-frontend-api.dll`, Qt6).

## Building from source

Requirements: Windows 10/11 x64, Visual Studio 2019/2022 (MSVC x64), CMake 3.16+, an installed OBS SDK and a Qt6 CMake package. The paths are set at the top of `CMakeLists.txt`:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The resulting `build/Release/SceneWall.dll` is the plugin.

## License

MIT License

Copyright (c) 2026 Studio Krause

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Credits

- **Author:** Studio Krause
- **Model / development:** Tencent Hy4 Preview
- **Companion plugin:** [studio-no-transition-strip](https://github.com/studiokrause/studio-no-transition-strip)
