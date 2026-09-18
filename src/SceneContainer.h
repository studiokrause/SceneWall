#pragma once
// Container wrapping FlowLayout that accepts scene drags and shows an
// insertion indicator so scenes can be reordered inside a tab.

#include "FlowLayout.h"

#include <QDrag>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPainter>
#include <QPoint>
#include <QRect>

class SceneContainer : public QWidget {
    Q_OBJECT

public:
    static constexpr const char *MIME = "application/x-scenewall-scene";

    explicit SceneContainer(QWidget *parent = nullptr) : QWidget(parent)
    {
        m_layout = new FlowLayout(this, 6, 6, 6);
        setAcceptDrops(true);
    }

    FlowLayout *flowLayout() const { return m_layout; }

signals:
    void sceneDropped(const QString &sceneName, int index);

protected:
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasFormat(MIME)) {
            event->acceptProposedAction();
            updateIndicator(event->position().toPoint());
        }
    }

    void dragMoveEvent(QDragMoveEvent *event) override
    {
        if (event->mimeData()->hasFormat(MIME)) {
            event->acceptProposedAction();
            updateIndicator(event->position().toPoint());
        }
    }

    void dragLeaveEvent(QDragLeaveEvent *) override
    {
        m_dropIndex = -1;
        update();
    }

    void dropEvent(QDropEvent *event) override
    {
        if (!event->mimeData()->hasFormat(MIME))
            return;

        const QString name = QString::fromUtf8(event->mimeData()->data(MIME));
        const int index = computeDropIndex(event->position().toPoint());

        m_dropIndex = -1;
        update();
        event->acceptProposedAction();

        emit sceneDropped(name, index);
    }

    void paintEvent(QPaintEvent *) override
    {
        if (m_dropIndex < 0)
            return;

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(palette().color(QPalette::Highlight));

        const int count = m_layout->count();
        int x = 0;
        int top = 6;
        int bottom = height() - 6;

        if (count == 0) {
            x = 3;
        } else if (m_dropIndex >= count) {
            QRect last = m_layout->itemAt(count - 1)->geometry();
            x = last.right() + 3;
            top = last.top();
            bottom = last.bottom();
        } else {
            QRect target = m_layout->itemAt(m_dropIndex)->geometry();
            x = target.left() - 3;
            top = target.top();
            bottom = target.bottom();
        }

        painter.drawRect(QRect(x, top, 3, bottom - top));
    }

private:
    void updateIndicator(const QPoint &pos)
    {
        const int idx = computeDropIndex(pos);
        if (idx != m_dropIndex) {
            m_dropIndex = idx;
            update();
        }
    }

    int computeDropIndex(const QPoint &pos) const
    {
        const int count = m_layout->count();
        for (int i = 0; i < count; ++i) {
            QLayoutItem *item = m_layout->itemAt(i);
            if (!item)
                continue;
            QRect r = item->geometry();

            if (pos.y() >= r.top() - 4 && pos.y() <= r.bottom() + 4) {
                if (pos.x() < r.center().x())
                    return i; // same row, left of this item's middle
            } else if (pos.y() < r.top()) {
                return i; // above this row
            }
        }
        return count;
    }

    FlowLayout *m_layout = nullptr;
    int m_dropIndex = -1;
};
