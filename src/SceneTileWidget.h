#pragma once

#include <QColor>
#include <QWidget>

#include "SceneWall.h"
#include "ThumbnailManager.h"

class SceneHeaderWidget;
class QVBoxLayout;

class SceneTileWidget : public QWidget, public IThumbnailReceiver {
	Q_OBJECT
public:
	explicit SceneTileWidget(obs_source_t *scene, QWidget *parent = nullptr);
	~SceneTileWidget() override;

	void setThumbSize(int size);
	void setHeaderColor(const QColor &color);
	void setThumbnailImage(const QImage &image) override;

	void setCollapsed(bool collapsed);
	bool isCollapsed() const { return collapsed; }
	void setRealtime(bool enable);
	bool isRealtime() const;

	obs_source_t *sceneSource() const { return scene; }
	QString sceneUuid() const;
	QString sceneName() const;

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

signals:
	void menuRequested(SceneTileWidget *tile, const QPoint &globalPos);

protected:
	bool eventFilter(QObject *watched, QEvent *event) override;
	void mousePressEvent(QMouseEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private slots:
	void toggleCollapse();

private:
	void applyCollapsedState();
	int expandedHeight() const;

	obs_source_t *scene;
	SceneHeaderWidget *header;
	SceneThumbnailWidget *thumb;
	QVBoxLayout *tileLayout;
	QColor barColor = QColor(0x80, 0x80, 0x80);
	int thumbSize = 160;
	bool collapsed = false;
};
