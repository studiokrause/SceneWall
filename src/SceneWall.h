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

// One scene cell: coloured header bar on top plus the live preview below it.
class SceneThumbnailWidget : public QWidget {
    Q_OBJECT
    obs_source_t *source;
    QTimer *refreshTimer;
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

public:
    SceneThumbnailWidget(obs_source_t *src, QWidget *parent = nullptr);
    ~SceneThumbnailWidget();

    void setThumbSize(int size);
    void setBarColor(const QColor &color);
    void setRefreshInterval(int ms);
    void setCollapsed(bool collapsed);
    void startTimer();
    void stopTimer();

    QString sceneName() const;
    bool collapsed() const { return isCollapsed; }
    obs_source_t *sourcePointer() const { return source; }

    bool isCollapsed = false;

    static constexpr int HEADER_HEIGHT = 22;
    static constexpr int COLLAPSED_WIDTH = 30;

signals:
    void menuRequested(SceneThumbnailWidget *widget, QPoint globalPos);
    void collapseToggled(const QString &sceneName, bool collapsed);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    QSize sizeHint() const override;

private slots:
    void updateThumbnail();

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

private slots:
    void openSettings();
    void applyThumbSize(int size);
    void runAutosize();
    void onTabMoved(int from, int to);
    void onSceneDropped(const QString &sceneName, int index);
    void onSceneMenu(SceneThumbnailWidget *widget, QPoint globalPos);
    void onRealtimeToggled(bool on);
    void onCollapseToggled(const QString &sceneName, bool collapsed);
    void reloadFromObs();

private:
    SceneTabWidget *tabContainer = nullptr;
    QSlider *sizeSlider = nullptr;
    QPushButton *autosizeBtn = nullptr;
    ToggleSwitch *realtimeToggle = nullptr;
    QPushButton *settingsBtn = nullptr;

    WallConfig config;

    // Collapse state survives tab rebuilds (e.g. saving Settings).
    QSet<QString> m_collapsedScenes;

    void loadTabs();
    void reflowAll();
    void syncWithObs(const QStringList &obsScenes);
    void refreshTabColors();
    QString currentTabId() const;
    SceneContainer *currentContainer() const;
    void assignSceneToTab(const QString &sceneName, const QString &tabId);
    void removeSceneFromTab(const QString &sceneName, const QString &tabId);
    void moveSceneInTab(const QString &sceneName, int index);
    int computeAutosize() const;

    int realtimeIntervalMs() const;
    void setTimersRunning(bool running);
};
