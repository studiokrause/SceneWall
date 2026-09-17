#pragma once
#include <QDockWidget>
#include <QWidget>
#include <QTimer>
#include <QMouseEvent>
#include <QTabWidget>
#include <QSlider>
#include <QPushButton>
#include <QLabel>
#include <obs.h>
#include <obs-audio-controls.h>

class SceneThumbnailWidget : public QWidget {
    Q_OBJECT
    obs_source_t* source;
    QTimer *refreshTimer;
    QImage thumbnail;
    bool realtime = false;
    bool showAudio = false;
    bool muted = false;
    obs_volmeter_t *volmeter = nullptr;
    int thumbSize = 160;
public:
    SceneThumbnailWidget(obs_source_t* src, QWidget *parent = nullptr);
    ~SceneThumbnailWidget();
    void setRealtime(bool enable);
    void setShowAudio(bool enable);
    void setThumbSize(int size);
    QString sceneName() const;
    bool isCollapsed = false;
    float audioLevel = 0.0f;
    QPoint dragStartPos;
    bool dragStarted = false;
protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
private slots:
    void updateThumbnail();
    void showContextMenu(const QPoint &pos);
};

class SceneWallWidget : public QDockWidget {
    Q_OBJECT
public:
    SceneWallWidget(QWidget *parent = nullptr);
private slots:
    void openSettings();
    void onOrderChanged(const QString &tabId, const QStringList &names);
private:
    QTabWidget *tabContainer;
    QSlider *sizeSlider;
    QPushButton *settingsBtn;
    QPushButton *aboutBtn;
    void loadTabs();
    void rebuildScenes();
};
