#pragma once
// Flow layout: lays out items left to right and wraps them onto the next line
// when the available width is exhausted (like CSS "flex-wrap: wrap").

#include <QLayout>
#include <QWidgetItem>
#include <QWidget>
#include <QStyle>
#include <QList>
#include <QRect>
#include <QPoint>
#include <QSize>
#include <QMargins>

class FlowLayout : public QLayout {
public:
    explicit FlowLayout(QWidget *parent, int margin = -1, int hSpacing = -1, int vSpacing = -1)
        : QLayout(parent), m_hSpace(hSpacing), m_vSpace(vSpacing)
    {
        setContentsMargins(margin, margin, margin, margin);
    }

    explicit FlowLayout(int margin = -1, int hSpacing = -1, int vSpacing = -1)
        : m_hSpace(hSpacing), m_vSpace(vSpacing)
    {
        setContentsMargins(margin, margin, margin, margin);
    }

    ~FlowLayout() override
    {
        QLayoutItem *item;
        while ((item = takeAt(0)))
            delete item;
    }

    void addItem(QLayoutItem *item) override { itemList.append(item); }

    int count() const override { return itemList.size(); }

    QLayoutItem *itemAt(int index) const override { return itemList.value(index); }

    QLayoutItem *takeAt(int index) override
    {
        if (index < 0 || index >= itemList.size())
            return nullptr;
        return itemList.takeAt(index);
    }

    Qt::Orientations expandingDirections() const override { return Qt::Orientations(); }

    bool hasHeightForWidth() const override { return true; }

    int heightForWidth(int width) const override { return doLayout(QRect(0, 0, width, 0), true); }

    void setGeometry(const QRect &rect) override
    {
        QLayout::setGeometry(rect);
        doLayout(rect, false);
    }

    QSize sizeHint() const override { return minimumSize(); }

    QSize minimumSize() const override
    {
        QSize size;
        for (QLayoutItem *item : itemList)
            size = size.expandedTo(item->minimumSize());

        QMargins margins = contentsMargins();
        size += QSize(margins.left() + margins.right(), margins.top() + margins.bottom());
        return size;
    }

    int horizontalSpacing() const
    {
        if (m_hSpace >= 0)
            return m_hSpace;
        return smartSpacing(QStyle::PM_LayoutHorizontalSpacing);
    }

    int verticalSpacing() const
    {
        if (m_vSpace >= 0)
            return m_vSpace;
        return smartSpacing(QStyle::PM_LayoutVerticalSpacing);
    }

private:
    int doLayout(const QRect &rect, bool testOnly) const
    {
        int left, top, right, bottom;
        getContentsMargins(&left, &top, &right, &bottom);
        QRect effectiveRect = rect.adjusted(+left, +top, -right, -bottom);
        int x = effectiveRect.x();
        int y = effectiveRect.y();
        int lineHeight = 0;

        for (QLayoutItem *item : itemList) {
            if (!item)
                continue;

            QWidget *wid = item->widget();
            int spaceX = horizontalSpacing();
            if (spaceX == -1)
                spaceX = wid ? wid->style()->layoutSpacing(QSizePolicy::PushButton,
                                                           QSizePolicy::PushButton, Qt::Horizontal)
                             : 0;
            int spaceY = verticalSpacing();
            if (spaceY == -1)
                spaceY = wid ? wid->style()->layoutSpacing(QSizePolicy::PushButton,
                                                           QSizePolicy::PushButton, Qt::Vertical)
                             : 0;

            QSize hint = item->sizeHint();
            int nextX = x + hint.width() + spaceX;

            // Wrap onto a new line when the item no longer fits.
            if (nextX - spaceX > effectiveRect.right() && lineHeight > 0) {
                x = effectiveRect.x();
                y = y + lineHeight + spaceY;
                nextX = x + hint.width() + spaceX;
                lineHeight = 0;
            }

            if (!testOnly)
                item->setGeometry(QRect(QPoint(x, y), hint));

            x = nextX;
            lineHeight = qMax(lineHeight, hint.height());
        }
        return y + lineHeight - rect.y() + bottom;
    }

    int smartSpacing(QStyle::PixelMetric pm) const
    {
        QObject *parent = this->parent();
        if (!parent)
            return -1;
        if (parent->isWidgetType()) {
            QWidget *pw = static_cast<QWidget *>(parent);
            return pw->style()->pixelMetric(pm, nullptr, pw);
        }
        return static_cast<QLayout *>(parent)->spacing();
    }

    QList<QLayoutItem *> itemList;
    int m_hSpace;
    int m_vSpace;
};
