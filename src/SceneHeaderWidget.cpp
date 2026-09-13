#include "SceneHeaderWidget.h"

#include <QMouseEvent>
#include <QPainter>

SceneHeaderWidget::SceneHeaderWidget(const QString &title, QWidget *parent) : QWidget(parent), title(title)
{
	setFixedHeight(20);
	setCursor(Qt::PointingHandCursor);
}

void SceneHeaderWidget::setBackgroundColor(const QColor &color)
{
	bgColor = color;
	update();
}

void SceneHeaderWidget::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.fillRect(rect(), bgColor);

	painter.setPen(Qt::white);
	const QString elided = fontMetrics().elidedText(title, Qt::ElideRight, width() - 6);
	painter.drawText(rect(), Qt::AlignCenter, elided);
}

void SceneHeaderWidget::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::RightButton) {
		emit collapseRequested();
		event->accept();
		return;
	}

	QWidget::mousePressEvent(event);
}
