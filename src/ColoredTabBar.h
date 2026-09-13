#pragma once

#include <QTabBar>
#include <QTabWidget>

class ColoredTabBar : public QTabBar {
	Q_OBJECT
public:
	explicit ColoredTabBar(QWidget *parent = nullptr);

protected:
	void paintEvent(QPaintEvent *event) override;
};

// QTabWidget::setTabBar jest protected — ta podklasa ustawia kolorowy pasek u siebie.
class ColoredTabWidget : public QTabWidget {
	Q_OBJECT
public:
	explicit ColoredTabWidget(QWidget *parent = nullptr);
};
