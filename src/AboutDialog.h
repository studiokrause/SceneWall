#pragma once
#include <QDialog>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

// Kept in sync with README.md.
static const char *SCENEWALL_ABOUT_TEXT = R"README(SceneWall - OBS Studio Plugin

SceneWall brings the vMix multiview idea to OBS Studio: a dockable panel where
every scene becomes a live thumbnail you can switch with a single click. It is
not a clone of vMix's source list - SceneWall adds user-defined tab groups with
custom colors, per-scene headers, collapsible scene bars, a wrapping (flex-like)
layout, one-click Autosize, drag & drop reordering and full localization.

WHAT IT IS
A dock panel that shows the scenes of the current scene collection as live
thumbnails in a wrapping grid that reflows to the width of the dock. Built for
operators who work with many scenes (cameras, overlays, helpers) and want to see
them all at once.

FEATURES
- Live scene thumbnails rendered through the OBS graphics subsystem.
- Full grabbing of all sources - every thumbnail keeps its scene showing
  (the same obs_source_inc_showing mechanism the native Multiview uses),
  so game capture, display/window capture, browser and media sources stay
  live even when the scene is neither in Preview nor Program.
- Wrapping layout - thumbnails and collapsed bars reflow like a flex container.
- Autosize - computes the largest size that fits every scene of the current tab
  in the visible area, then applies it to all tabs. Collapsed bars count as
  narrower, so packing stays efficient.
- Realtime mode - refreshes at the frame rate OBS is currently running at, after
  a resource-usage confirmation.
- Optimized rendering - GPU scratch buffers are allocated once per thumbnail and
  reused, and anything not actually visible (hidden dock, other tab, scrolled
  out of view) is skipped.
- Tab groups with colors for headers and tab buttons.
- Scene assignment keyed by a stable tab ID.
- Drag & drop reordering of tabs and of scenes - each tab keeps its own order,
  independent of the scene list in OBS. Saved between sessions.
- Scene headers with white text on a configurable color; right-click to collapse
  a scene into a vertical bar, right-click again to expand.
- Program (red) and Preview (green) indicators.
- Localization: English, German, Polish, Ukrainian, Italian, Spanish, French.

MOUSE AND KEYBOARD
- Left click on a thumbnail: switch to that scene.
- Left drag on a thumbnail: reorder it inside the current tab.
- Right click on a scene header: collapse / expand that scene.
- Right click on a thumbnail: in Studio Mode set it as the Preview scene.
  A plain right-click never opens a menu.
- CTRL + click on a thumbnail: plugin menu (Assign to Tab, Remove from Tab,
  Collapse / Expand, Properties).

TABS AND ASSIGNMENT
- The All tab always shows every scene; its title can be renamed and cannot be
  deleted. On first run it uses the OBS interface language.
- Create, rename, recolor and remove tabs in Settings (gear button). Clicking a
  color cell opens a color picker.
- A scene belongs to one tab; unassigned scenes appear only in All.
- A scene's header takes the color of the tab it is assigned to, so its group is
  visible even while browsing All.

INSTALLATION
1. Copy SceneWall.dll to OBS_FOLDER/obs-plugins/64bit/
2. Copy data/obs-plugins/SceneWall/ (the locale folder) to
   OBS_FOLDER/data/obs-plugins/SceneWall/
3. Restart OBS Studio.

SceneWall uses only libraries shipped with OBS (obs.dll, obs-frontend-api.dll,
Qt6).
)README";

class AboutDialog : public QDialog {
public:
    AboutDialog(QWidget *parent = nullptr) : QDialog(parent)
    {
        setWindowTitle("About SceneWall");
        resize(660, 620);

        QVBoxLayout *layout = new QVBoxLayout(this);

        QLabel *title = new QLabel("SceneWall", this);
        title->setAlignment(Qt::AlignCenter);
        QFont font = title->font();
        font.setPointSize(16);
        font.setBold(true);
        title->setFont(font);
        layout->addWidget(title);

        QLabel *version = new QLabel("Version 1.3", this);
        version->setAlignment(Qt::AlignCenter);
        layout->addWidget(version);

        QTextEdit *info = new QTextEdit(this);
        info->setReadOnly(true);
        info->setLineWrapMode(QTextEdit::NoWrap);
        info->setPlainText(QString(SCENEWALL_ABOUT_TEXT));
        layout->addWidget(info, 1);

        QTextEdit *license = new QTextEdit(this);
        license->setReadOnly(true);
        license->setFixedHeight(150);
        license->setPlainText(
            "MIT License\n\n"
            "Copyright (c) 2026 Studio Krause\n\n"
            "Permission is hereby granted, free of charge, to any person obtaining a copy "
            "of this software and associated documentation files (the \"Software\"), to deal "
            "in the Software without restriction, including without limitation the rights "
            "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell "
            "copies of the Software, and to permit persons to whom the Software is "
            "furnished to do so, subject to the following conditions:\n\n"
            "The above copyright notice and this permission notice shall be included in all "
            "copies or substantial portions of the Software.\n\n"
            "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR "
            "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, "
            "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE "
            "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER "
            "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, "
            "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE "
            "SOFTWARE.");
        layout->addWidget(license);

        QLabel *credits = new QLabel("Studio Krause / Tencent Hy4 Preview", this);
        credits->setAlignment(Qt::AlignCenter);
        layout->addWidget(credits);

        QHBoxLayout *btnLayout = new QHBoxLayout();
        btnLayout->addStretch();
        QPushButton *closeBtn = new QPushButton("Close", this);
        btnLayout->addWidget(closeBtn);
        layout->addLayout(btnLayout);

        connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    }
};
