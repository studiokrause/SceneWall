#pragma once
#include <QWidget>
#include <QGridLayout>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QList>
#include <QString>
#include <QStringList>
#include <obs.h>

#include "SceneWall.h"

inline constexpr const char *kSceneMimeType = "application/x-scenewall-scene";

class SceneGridWidget : public QWidget {
    Q_OBJECT
public:
    SceneGridWidget(const QString &tabId, int thumbSize, QWidget *parent = nullptr);

    void setScenes(const QList<obs_source_t *> &scenes);
    void setThumbSize(int size);
    QStringList order() const;

signals:
    void orderChanged(const QString &tabId, const QStringList &names);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    int dropIndexAt(const QPoint &pos) const;
    void clearLayout();
    void relayout();

    QString tabId;
    QGridLayout *grid;
    QList<SceneThumbnailWidget *> items;
    int thumbSize;
    int dropIndex = -1;
    static const int maxCols = 4;
};
