#include "SceneGridWidget.h"
#include <QMimeData>
#include <QPainter>
#include <limits>

SceneGridWidget::SceneGridWidget(const QString &tabId, int thumbSize, QWidget *parent)
    : QWidget(parent), tabId(tabId), thumbSize(thumbSize)
{
    grid = new QGridLayout(this);
    grid->setSpacing(4);
    setAcceptDrops(true);
}

void SceneGridWidget::setScenes(const QList<obs_source_t *> &scenes) {
    clearLayout();
    qDeleteAll(items);
    items.clear();

    for (obs_source_t *src : scenes) {
        SceneThumbnailWidget *w = new SceneThumbnailWidget(src, this);
        w->setThumbSize(thumbSize);
        items.append(w);
    }

    relayout();
}

void SceneGridWidget::setThumbSize(int size) {
    thumbSize = size;
    for (SceneThumbnailWidget *w : items)
        w->setThumbSize(size);
    relayout();
}

QStringList SceneGridWidget::order() const {
    QStringList names;
    for (SceneThumbnailWidget *w : items)
        names.append(w->sceneName());
    return names;
}

void SceneGridWidget::clearLayout() {
    while (QLayoutItem *item = grid->takeAt(0)) {
        if (QWidget *w = item->widget())
            w->hide();
        delete item;
    }
}

void SceneGridWidget::relayout() {
    clearLayout();
    for (int i = 0; i < items.size(); ++i) {
        SceneThumbnailWidget *w = items[i];
        grid->addWidget(w, i / maxCols, i % maxCols);
        w->show();
    }
    updateGeometry();
    update();
}

int SceneGridWidget::dropIndexAt(const QPoint &pos) const {
    if (items.isEmpty())
        return 0;

    const QRect last = items.last()->geometry();
    if (pos.y() > last.bottom() || (pos.y() >= last.top() && pos.x() > last.right()))
        return items.size();

    int best = 0;
    long bestDist = std::numeric_limits<long>::max();
    for (int i = 0; i < items.size(); ++i) {
        QPoint c = items[i]->geometry().center();
        long dx = c.x() - pos.x();
        long dy = c.y() - pos.y();
        long dist = dx * dx + dy * dy;
        if (dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }

    return (pos.x() < items[best]->geometry().center().x()) ? best : best + 1;
}

void SceneGridWidget::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasFormat(kSceneMimeType)) {
        event->setDropAction(Qt::MoveAction);
        event->accept();
    } else {
        event->ignore();
    }
}

void SceneGridWidget::dragMoveEvent(QDragMoveEvent *event) {
    if (!event->mimeData()->hasFormat(kSceneMimeType)) {
        event->ignore();
        return;
    }

    dropIndex = dropIndexAt(event->position().toPoint());
    event->setDropAction(Qt::MoveAction);
    event->accept();
    update();
}

void SceneGridWidget::dragLeaveEvent(QDragLeaveEvent *) {
    dropIndex = -1;
    update();
}

void SceneGridWidget::dropEvent(QDropEvent *event) {
    dropIndex = -1;

    if (!event->mimeData()->hasFormat(kSceneMimeType)) {
        event->ignore();
        update();
        return;
    }

    const QString name = QString::fromUtf8(event->mimeData()->data(kSceneMimeType));

    int from = -1;
    for (int i = 0; i < items.size(); ++i) {
        if (items[i]->sceneName() == name) {
            from = i;
            break;
        }
    }

    if (from < 0) {
        event->ignore();
        update();
        return;
    }

    int to = dropIndexAt(event->position().toPoint());
    if (to > from)
        --to;

    event->setDropAction(Qt::MoveAction);
    event->accept();

    if (to != from) {
        items.move(from, to);
        relayout();
        emit orderChanged(tabId, order());
    } else {
        update();
    }
}

void SceneGridWidget::paintEvent(QPaintEvent *event) {
    QWidget::paintEvent(event);

    if (dropIndex < 0 || items.isEmpty())
        return;

    QRect r = (dropIndex < items.size()) ? items[dropIndex]->geometry() : items.last()->geometry();
    const int x = (dropIndex < items.size()) ? r.left() - 2 : r.right() + 2;

    QPainter painter(this);
    painter.setPen(QPen(QColor(255, 255, 255), 3));
    painter.drawLine(x, r.top(), x, r.bottom());
}
