#pragma once
#include "Config.h"
#include "SceneContainer.h"
#include "SceneTabBar.h"
#include "ToggleSwitch.h"

#include <QColor>
#include <QDockWidget>
#include <QHideEvent>
#include <QIcon>
#include <QImage>
#include <QLabel>
#include <QList>
#include <QMouseEvent>
#include <QShowEvent>
#include <QPoint>
#include <QPushButton>
#include <QSet>
#include <QSize>
#include <QSlider>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>

#include <obs.h>
#include <obs-frontend-api.h>
#include <graphics/graphics.h>

// Shared render-budget policy for the wall: one QTimer in SceneWallWidget
// walks the thumbnails round-robin, at most maxGpuReadsPerTick per tick.
// Hidden/collapsed/inactive items are skipped without consuming budget.
// Pure logic (no OBS/Qt-widget calls) so it is unit-testable.
struct ThumbnailScheduler {
    static constexpr int DEFAULT_MAX_READS = 2;
    static constexpr int NORMAL_INTERVAL_MS = 500;
    static constexpr int REALTIME_FALLBACK_MS = 33;

    static int realtimeIntervalForFps(double fps)
    {
        if (fps <= 0.0)
            return REALTIME_FALLBACK_MS;
        const int ms = (int)(1000.0 / fps + 0.5);
        return ms < 1 ? 1 : ms;
    }

    int maxReads = DEFAULT_MAX_READS;
    int cursor = 0;

    // Returns indices of up to maxReads eligible items, continuing from the
    // previous tick so every visible widget is reached in turn.
    QList<int> pickNext(const QList<bool> &eligible)
    {
        QList<int> picked;
        const int n = eligible.size();
        if (n <= 0 || maxReads <= 0)
            return picked;
        if (cursor < 0 || cursor >= n)
            cursor = 0;
        int pos = cursor;
        for (int visited = 0; visited < n && picked.size() < maxReads; ++visited) {
            if (eligible[pos])
                picked.append(pos);
            pos = (pos + 1) % n;
        }
        cursor = pos;
        return picked;
    }
};

// One scene cell: coloured header bar on top plus the live preview below it.
// Holds one obs source ref for its whole lifetime; ctor balances
// obs_source_inc_showing() with exactly one dec_showing before release in dtor.
// Has no timer of its own: the wall scheduler calls renderIfVisible().
class SceneThumbnailWidget : public QWidget {
    Q_OBJECT
    obs_source_t *source;
    QImage thumbnail;
    int thumbSize = 160;
    QColor barColor = QColor(80, 80, 80);
    QPoint dragStartPos;
    bool dragStarted = false;

    // Reused GPU scratch buffers: allocating these per frame is expensive.
    gs_texrender_t *texrender = nullptr;
    gs_stagesurf_t *stagesurf = nullptr;
    int stageW = 0;
    int stageH = 0;

    // Program / Preview state is pushed down by the wall instead of being
    // queried from OBS on every repaint.
    bool isProgram = false;
    bool isPreview = false;

public:
    SceneThumbnailWidget(obs_source_t *src, QWidget *parent = nullptr);
    ~SceneThumbnailWidget();

    void setThumbSize(int size);
    void setBarColor(const QColor &color);
    void setCollapsed(bool collapsed);
    void setProgram(bool on);
    void setPreview(bool on);
    void renderIfVisible(); // wall scheduler entry: skips viewport-hidden

    QString sceneName() const;
    QString sceneUuid() const;
    bool collapsed() const { return isCollapsed; }
    obs_source_t *sourcePointer() const { return source; }

    bool isCollapsed = false;

    static constexpr int HEADER_HEIGHT = 22;
    static constexpr int COLLAPSED_WIDTH = 30;

signals:
    void menuRequested(SceneThumbnailWidget *widget, QPoint globalPos);
    void collapseToggled(const QString &sceneUuid, bool collapsed);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    QSize sizeHint() const override;

private:
    void applySize();
    QRect headerRect() const;
    QRect previewRect() const;
    QSize previewSize() const;
    void renderPreview();
    bool renderAsyncFrame();
    bool renderSceneOffscreen();
    bool isVisibleInViewport() const;
};

class SceneWallWidget : public QDockWidget {
    Q_OBJECT
public:
    SceneWallWidget(QWidget *parent = nullptr);
    ~SceneWallWidget() override;

    // Watches OBS for scenes being added, removed or renamed.
    static void onFrontendEvent(enum obs_frontend_event event, void *param);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void changeEvent(QEvent *event) override;

private slots:
    void openSettings();
    void applyThumbSize(int size);
    void runAutosize();
    void onTabMoved(int from, int to);
    void onSceneDropped(const QString &sceneUuid, int index);
    void onSceneMenu(SceneThumbnailWidget *widget, QPoint globalPos);
    void onRealtimeToggled(bool on);
    void onCollapseToggled(const QString &sceneUuid, bool collapsed);
    void reloadFromObs();
    void updateIndicators();
    void onSchedulerTick();

private:
    SceneTabWidget *tabContainer = nullptr;
    QSlider *sizeSlider = nullptr;
    QPushButton *autosizeBtn = nullptr;
    ToggleSwitch *realtimeToggle = nullptr;
    QPushButton *settingsBtn = nullptr;

    WallConfig config;
    QString m_collectionId;

    // Cached list of live thumbnails (rebuilt by loadTabs) so indicator
    // updates and size changes don't have to walk the widget tree.
    QList<SceneThumbnailWidget *> m_thumbWidgets;

    // One shared render timer for the whole wall (see ThumbnailScheduler).
    QTimer *m_renderTimer = nullptr;
    ThumbnailScheduler m_schedule;

    void loadTabs();
    void reflowAll();
    bool persistConfig(); // saveWallConfig + warning on failure, so edits are never lost silently
    void syncWithObs(const QStringList &obsSceneUuids,
                     const QMap<QString, QStringList> &uuidsBySceneName);
    CollectionConfig &currentCollection();
    void refreshTabColors();
    QString currentTabId() const;
    void assignSceneToTab(const QString &sceneUuid, const QString &tabId);
    void removeSceneFromTab(const QString &sceneUuid, const QString &tabId);
    void moveSceneInTab(const QString &sceneUuid, int index);
    int computeAutosize() const;

    int realtimeIntervalMs() const;
    void startScheduler();
    void stopScheduler();
    void refreshGearIcon();
};
