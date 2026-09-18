#pragma once
// Tab bar with per-tab background colour, white text and rounded top corners.

#include <QColor>
#include <QFontMetrics>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QRect>
#include <QTabBar>
#include <QTabWidget>

class SceneTabBar : public QTabBar {
    Q_OBJECT

public:
    // Horizontal gap painted between neighbouring tabs.
    static constexpr int TAB_GAP = 3;

public:
    explicit SceneTabBar(QWidget *parent = nullptr) : QTabBar(parent)
    {
        setMovable(true);            // drag & drop reordering of tabs
        setUsesScrollButtons(true);  // scroll arrows = "there are more tabs"
        setExpanding(false);
        setDrawBase(false);
        setElideMode(Qt::ElideRight);
    }

    void setTabColor(int index, const QColor &color)
    {
        m_colors.insert(index, color);
        update();
    }

    void clearColors()
    {
        m_colors.clear();
        update();
    }

    QColor tabColor(int index) const { return m_colors.value(index); }

protected:
    QSize tabSizeHint(int index) const override
    {
        QSize s = QTabBar::tabSizeHint(index);
        s.setWidth(qMax(s.width() + 12, 72) + TAB_GAP);
        s.setHeight(qMax(s.height(), 24));
        return s;
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);

        for (int i = 0; i < count(); ++i) {
            // Inset horizontally so neighbouring tabs are visibly separated.
            QRect r = tabRect(i).adjusted(TAB_GAP / 2, 0, -(TAB_GAP / 2), 0);
            if (!r.isValid() || !r.intersects(rect()))
                continue;

            const bool selected = (i == currentIndex());

            QColor base = m_colors.value(i);
            if (!base.isValid())
                base = QColor(90, 90, 90);

            // Selected tab reads brighter; unselected slightly recessed.
            QColor fill = selected ? base : base.darker(135);

            QPainterPath path;
            const qreal radius = 5.0;
            path.moveTo(r.left(), r.bottom());
            path.lineTo(r.left(), r.top() + radius);
            path.quadTo(QPointF(r.left(), r.top()), QPointF(r.left() + radius, r.top()));
            path.lineTo(r.right() - radius, r.top());
            path.quadTo(QPointF(r.right(), r.top()), QPointF(r.right(), r.top() + radius));
            path.lineTo(r.right(), r.bottom());
            path.closeSubpath();

            painter.setBrush(fill);
            painter.drawPath(path);

            painter.setPen(Qt::white);
            painter.drawText(r, Qt::AlignCenter,
                             fontMetrics().elidedText(tabText(i), Qt::ElideRight, r.width() - 14));
            painter.setPen(Qt::NoPen);
        }
    }

private:
    QMap<int, QColor> m_colors;
};

// QTabWidget::setTabBar is protected, so the custom bar is installed here.
class SceneTabWidget : public QTabWidget {
    Q_OBJECT

public:
    explicit SceneTabWidget(QWidget *parent = nullptr) : QTabWidget(parent)
    {
        setTabBar(new SceneTabBar(this));
    }

    SceneTabBar *sceneTabBar() const { return qobject_cast<SceneTabBar *>(tabBar()); }
};
