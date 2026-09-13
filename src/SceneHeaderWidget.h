#pragma once

#include <QColor>
#include <QWidget>

class SceneHeaderWidget : public QWidget {
	Q_OBJECT
public:
	explicit SceneHeaderWidget(const QString &title, QWidget *parent = nullptr);

	void setBackgroundColor(const QColor &color);
	QColor backgroundColor() const { return bgColor; }

signals:
	void collapseRequested();

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;

private:
	QString title;
	QColor bgColor = QColor(0x80, 0x80, 0x80);
};
