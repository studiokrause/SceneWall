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
#include <QPoint>
#include <QRect>
#include <QWidget>

class SceneContainer : public QWidget {
    Q_OBJECT

public:
    static constexpr const char *MIME = "application/x-scenewall-scene";

    explicit SceneContainer(QWidget *parent = nullptr) : QWidget(parent)
    {
        m_layout = new FlowLayout(this, 6, 6, 6);
        setAcceptDrops(true);

        // The insertion marker is a raised child widget rather than something
        // painted in paintEvent(): a parent paints before its children, so a
        // painted marker would be covered by the thumbnails.
        m_indicator = new QWidget(this);
        m_indicator->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_indicator->hide();
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

    void dragLeaveEvent(QDragLeaveEvent *) override { hideIndicator(); }

    void dropEvent(QDropEvent *event) override
    {
        if (!event->mimeData()->hasFormat(MIME))
            return;

        const QString name = QString::fromUtf8(event->mimeData()->data(MIME));
        const int index = computeDropIndex(event->position().toPoint());

        hideIndicator();
        event->acceptProposedAction();

        emit sceneDropped(name, index);
    }

private:
    void hideIndicator()
    {
        m_dropIndex = -1;
        if (m_indicator)
            m_indicator->hide();
    }

    void updateIndicator(const QPoint &pos)
    {
        const int idx = computeDropIndex(pos);
        if (idx == m_dropIndex)
            return;
        m_dropIndex = idx;

        const int count = m_layout->count();
        int x = 3;
        int top = 6;
        int bottom = height() - 6;

        if (count > 0) {
            if (m_dropIndex >= count) {
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
        }

        if (!m_indicator)
            return;

        m_indicator->setStyleSheet(
                QString("background-color: %1;")
                        .arg(palette().color(QPalette::Highlight).name()));
        m_indicator->setGeometry(x, top, 3, bottom - top);
        m_indicator->raise();
        m_indicator->show();
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
    QWidget *m_indicator = nullptr;
    int m_dropIndex = -1;
};
