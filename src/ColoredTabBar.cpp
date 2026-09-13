#include "ColoredTabBar.h"

#include <QPainter>
#include <QPainterPath>

namespace {
constexpr int kCornerRadius = 4;
constexpr int kSideGap = 1;
} // namespace

ColoredTabBar::ColoredTabBar(QWidget *parent) : QTabBar(parent)
{
}

ColoredTabWidget::ColoredTabWidget(QWidget *parent) : QTabWidget(parent)
{
	setTabBar(new ColoredTabBar(this));
}

void ColoredTabBar::paintEvent(QPaintEvent *event)
{
	// Najpierw rysujemy bazowo (m.in. strzałki przewijania i układ zakładek),
	// potem nakładamy kolorowe tła z zaokrąglonymi górnymi rogami i biały tekst.
	QTabBar::paintEvent(event);

	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	for (int i = 0; i < count(); ++i) {
		const QRect tab = tabRect(i);
		if (!tab.isValid())
			continue;

		const QColor color = tabData(i).value<QColor>();
		if (!color.isValid())
			continue;

		const QRect r = tab.adjusted(kSideGap, 0, -kSideGap, 0);

		QPainterPath path;
		path.moveTo(r.left(), r.bottom() + 1);
		path.lineTo(r.left(), r.top() + kCornerRadius);
		path.quadTo(r.left(), r.top(), r.left() + kCornerRadius, r.top());
		path.lineTo(r.right() - kCornerRadius, r.top());
		path.quadTo(r.right(), r.top(), r.right(), r.top() + kCornerRadius);
		path.lineTo(r.right(), r.bottom() + 1);
		path.closeSubpath();

		painter.fillPath(path, color);

		painter.setPen(Qt::white);
		painter.drawText(r, Qt::AlignCenter, tabText(i));

		if (i == currentIndex()) {
			painter.setPen(QPen(Qt::white, 2));
			painter.drawPath(path);
		}
	}
}
