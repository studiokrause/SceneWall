#include "ToggleSwitch.h"

#include <QPainter>

ToggleSwitch::ToggleSwitch(QWidget *parent) : QAbstractButton(parent)
{
	setCheckable(true);
	setCursor(Qt::PointingHandCursor);
	setFixedSize(sizeHint());
}

QSize ToggleSwitch::sizeHint() const
{
	return QSize(40, 20);
}

QSize ToggleSwitch::minimumSizeHint() const
{
	return sizeHint();
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
	QPainter painter(this);
	painter.setRenderHint(QPainter::Antialiasing);

	const QRect track = rect().adjusted(0, 1, 0, -1);
	const QColor trackColor = isChecked() ? QColor(0, 170, 80) : QColor(90, 90, 90);

	painter.setPen(Qt::NoPen);
	painter.setBrush(trackColor);
	painter.drawRoundedRect(track, track.height() / 2.0, track.height() / 2.0);

	const int knobSize = track.height() - 4;
	const int knobX = isChecked() ? track.right() - knobSize - 2 : track.left() + 2;
	const QRect knob(knobX, track.top() + 2, knobSize, knobSize);

	painter.setBrush(Qt::white);
	painter.drawEllipse(knob);
}
