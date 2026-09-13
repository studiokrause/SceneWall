#pragma once

#include <QDockWidget>
#include <QList>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QSlider>
#include <QTabWidget>
#include <QTimer>
#include <QWidget>

#include <obs.h>

#include "SceneWallConfig.h"

class SceneTileWidget;
class ToggleSwitch;

class SceneThumbnailWidget : public QWidget {
	Q_OBJECT
public:
	SceneThumbnailWidget(obs_source_t *src, QWidget *parent = nullptr);
	~SceneThumbnailWidget() override;

	void setThumbSize(int size);
	void setThumbnail(const QPixmap &pixmap);
	void setRealtime(bool enable);

	bool realtime = false;

protected:
	void paintEvent(QPaintEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;

private slots:
	void refreshState();

private:
	obs_source_t *source;
	QTimer *stateTimer;
	QPixmap thumbnail;
	int thumbSize = 160;
};

class SceneWallWidget : public QDockWidget {
	Q_OBJECT
public:
	SceneWallWidget(QWidget *parent = nullptr);

private slots:
	void openSettings();
	void onMenuRequested(SceneTileWidget *tile, const QPoint &globalPos);
	void setAllRealtime(bool enable);
	void autoSize();

private:
	QTabWidget *tabContainer;
	QSlider *sizeSlider;
	QPushButton *autosizeBtn;
	ToggleSwitch *realtimeToggle;
	QPushButton *settingsBtn;
	QList<SceneTileWidget *> tiles;

	void loadTabs();
	void applyThumbSize(int size);
	void renameScene(SceneTileWidget *tile);
	void duplicateScene(SceneTileWidget *tile);
	void assignSceneToTab(SceneTileWidget *tile, const QString &tabName, bool isAllTab);
	void rebuildLater();
};
