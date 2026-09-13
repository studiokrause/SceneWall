#include "SceneWall.h"

#include "AboutDialog.h"
#include "ColoredTabBar.h"
#include "FlowLayout.h"
#include "Locale.h"
#include "SceneTileWidget.h"
#include "SceneWallConfig.h"
#include "SettingsDialog.h"
#include "ThumbnailManager.h"
#include "ToggleSwitch.h"

#include <QAction>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QScrollArea>
#include <QVBoxLayout>

#include <obs-frontend-api.h>
#include <obs-source.h>

SceneThumbnailWidget::SceneThumbnailWidget(obs_source_t *src, QWidget *parent)
	: QWidget(parent), source(obs_source_get_ref(src))
{
	setFixedSize(thumbSize, thumbSize * 9 / 16);

	stateTimer = new QTimer(this);
	connect(stateTimer, &QTimer::timeout, this, &SceneThumbnailWidget::refreshState);
	stateTimer->start(250);
}

SceneThumbnailWidget::~SceneThumbnailWidget()
{
	if (source)
		obs_source_release(source);
}

void SceneThumbnailWidget::setThumbSize(int size)
{
	thumbSize = size;
	setFixedSize(thumbSize, thumbSize * 9 / 16);
	update();
}

void SceneThumbnailWidget::setThumbnail(const QPixmap &pixmap)
{
	thumbnail = pixmap;
	update();
}

void SceneThumbnailWidget::setRealtime(bool enable)
{
	realtime = enable;
	ThumbnailManager::instance().setIntervalMs(enable ? 33 : 500);
}

void SceneThumbnailWidget::refreshState()
{
	update();
}

void SceneThumbnailWidget::paintEvent(QPaintEvent *)
{
	QPainter painter(this);

	if (!thumbnail.isNull()) {
		painter.drawPixmap(0, 0, thumbnail);
	} else {
		painter.fillRect(rect(), Qt::black);
	}

	obs_source_t *currentScene = obs_frontend_get_current_scene();
	if (currentScene) {
		if (currentScene == source) {
			painter.setPen(QPen(QColor(255, 0, 0), 3));
			painter.drawRect(0, 0, width() - 1, height() - 1);
		}
		obs_source_release(currentScene);
	}

	if (obs_frontend_preview_program_mode_active()) {
		obs_source_t *previewScene = obs_frontend_get_current_preview_scene();
		if (previewScene) {
			if (previewScene == source) {
				painter.setPen(QPen(QColor(0, 255, 0), 3));
				painter.drawRect(0, 0, width() - 1, height() - 1);
			}
			obs_source_release(previewScene);
		}
	}
}

void SceneThumbnailWidget::mousePressEvent(QMouseEvent *event)
{
	if (event->button() == Qt::LeftButton) {
		obs_frontend_set_current_scene(source);
	} else if (event->button() == Qt::RightButton) {
		if (obs_frontend_preview_program_mode_active())
			obs_frontend_set_current_preview_scene(source);
	}
}

SceneWallWidget::SceneWallWidget(QWidget *parent) : QDockWidget(parent)
{
	setWindowTitle("SceneWall");

	auto *content = new QWidget(this);
	auto *layout = new QVBoxLayout(content);

	auto *toolbar = new QWidget(this);
	auto *toolbarLayout = new QHBoxLayout(toolbar);
	// Minimalny odstęp z prawej strony (od zakładek i od krawędzi).
	toolbarLayout->setContentsMargins(12, 0, 4, 0);

	sizeSlider = new QSlider(Qt::Horizontal, toolbar);
	sizeSlider->setRange(50, 300);
	sizeSlider->setValue(160);
	sizeSlider->setFixedWidth(120);

	autosizeBtn = new QPushButton(T("Autosize"), toolbar);
	realtimeToggle = new ToggleSwitch(toolbar);
	realtimeToggle->setToolTip(T("Realtime"));
	settingsBtn = new QPushButton(QString::fromUtf8("\xE2\x9A\x99"), toolbar);
	settingsBtn->setToolTip(T("Settings"));

	toolbarLayout->addWidget(new QLabel(T("Size") + ":", toolbar));
	toolbarLayout->addWidget(sizeSlider);
	toolbarLayout->addWidget(autosizeBtn);
	toolbarLayout->addWidget(new QLabel(T("Realtime"), toolbar));
	toolbarLayout->addWidget(realtimeToggle);
	toolbarLayout->addWidget(settingsBtn);

	tabContainer = new ColoredTabWidget(this);
	tabContainer->setUsesScrollButtons(true);
	tabContainer->setElideMode(Qt::ElideRight);
	tabContainer->tabBar()->setExpanding(false);
	tabContainer->setCornerWidget(toolbar, Qt::TopRightCorner);

	layout->addWidget(tabContainer);

	loadTabs();

	connect(sizeSlider, &QSlider::valueChanged, this, &SceneWallWidget::applyThumbSize);
	connect(autosizeBtn, &QPushButton::clicked, this, &SceneWallWidget::autoSize);
	connect(realtimeToggle, &ToggleSwitch::toggled, this, &SceneWallWidget::setAllRealtime);
	connect(settingsBtn, &QPushButton::clicked, this, &SceneWallWidget::openSettings);

	setWidget(content);
}

void SceneWallWidget::applyThumbSize(int size)
{
	for (SceneTileWidget *tile : tiles)
		tile->setThumbSize(size);
}

void SceneWallWidget::setAllRealtime(bool enable)
{
	if (enable) {
		QMessageBox::warning(this, T("HighResourceTitle"), T("HighResourceWarning"));
	}
	ThumbnailManager::instance().setIntervalMs(enable ? 33 : 500);
	for (SceneTileWidget *tile : tiles)
		tile->setRealtime(enable);
}

void SceneWallWidget::autoSize()
{
	auto *scroll = qobject_cast<QScrollArea *>(tabContainer->currentWidget());
	if (!scroll || !scroll->widget())
		return;

	const QSize available = scroll->viewport()->size();
	const QList<SceneTileWidget *> tabTiles = scroll->widget()->findChildren<SceneTileWidget *>();
	if (tabTiles.isEmpty() || available.width() <= 0 || available.height() <= 0)
		return;

	const int spacing = 6;
	const int margin = 6;
	const int collapsedWidth = 28;
	const int usableWidth = available.width() - 2 * margin;
	int best = 50;

	for (int size = 300; size >= 50; --size) {
		const int tileHeight = 20 + size * 9 / 16;

		// Symulacja przepływu: zwinięte belki zajmują mniej miejsca w poziomie.
		int rows = 1;
		int x = 0;
		bool tooWide = false;
		for (SceneTileWidget *tile : tabTiles) {
			const int tileWidth = tile->isCollapsed() ? collapsedWidth : size;
			if (tileWidth > usableWidth) {
				tooWide = true;
				break;
			}
			if (x > 0 && x + tileWidth > usableWidth) {
				rows++;
				x = 0;
			}
			x += tileWidth + spacing;
		}
		if (tooWide)
			continue;

		const int neededHeight = rows * tileHeight + (rows - 1) * spacing + 2 * margin;
		if (neededHeight <= available.height()) {
			best = size;
			break;
		}
	}

	sizeSlider->setValue(best);
}

void SceneWallWidget::loadTabs()
{
	tiles.clear();
	tabContainer->clear();

	SceneWallConfigData config = loadSceneWallConfig();
	if (config.migrated)
		saveSceneWallConfig(config);
	const int thumbSize = sizeSlider ? sizeSlider->value() : 160;

	struct SceneEntry {
		obs_source_t *source;
		QString uuid;
	};

	QList<SceneEntry> scenes;
	struct obs_frontend_source_list sceneList = {};
	obs_frontend_get_scenes(&sceneList);
	for (size_t i = 0; i < sceneList.sources.num; i++) {
		obs_source_t *source = sceneList.sources.array[i];
		scenes.append({source, QString::fromUtf8(obs_source_get_uuid(source))});
	}

	for (const SceneWallTab &tab : config.tabs) {
		auto *scroll = new QScrollArea();
		scroll->setWidgetResizable(true);

		auto *tabWidget = new QWidget();
		auto *flow = new FlowLayout(tabWidget, 6, 6, 6);

		for (const SceneEntry &entry : scenes) {
			const QString assigned = config.assignments.value(entry.uuid);
			if (!tab.isAll && assigned != tab.id)
				continue;

			auto *tile = new SceneTileWidget(entry.source, tabWidget);
			tile->setHeaderColor(colorForScene(config, entry.uuid));
			tile->setRealtime(realtimeToggle && realtimeToggle->isChecked());
			tile->setThumbSize(thumbSize);
			connect(tile, &SceneTileWidget::menuRequested, this, &SceneWallWidget::onMenuRequested);
			flow->addWidget(tile);
			tiles.append(tile);
		}

		scroll->setWidget(tabWidget);
		const int index = tabContainer->addTab(scroll, tab.name);

		QColor tabColor(tab.color);
		if (!tabColor.isValid())
			tabColor = QColor(defaultHeaderColor());
		tabContainer->tabBar()->setTabData(index, tabColor);
	}

	obs_frontend_source_list_free(&sceneList);
}

void SceneWallWidget::onMenuRequested(SceneTileWidget *tile, const QPoint &globalPos)
{
	if (!tile)
		return;

	obs_source_t *source = tile->sceneSource();

	QMenu menu;
	QAction *renameAction = menu.addAction(T("Rename"));
	QAction *duplicateAction = menu.addAction(T("Duplicate"));
	QAction *removeAction = menu.addAction(T("Remove"));
	menu.addSeparator();
	QAction *filtersAction = menu.addAction(T("Filters"));
	QAction *propertiesAction = menu.addAction(T("Properties"));
	menu.addSeparator();

	const SceneWallConfigData config = loadSceneWallConfig();
	const QString currentTabId = config.assignments.value(tile->sceneUuid());

	QMenu *assignMenu = menu.addMenu(T("AssignToTab"));
	for (const SceneWallTab &tab : config.tabs) {
		QAction *action = assignMenu->addAction(tab.name);
		action->setCheckable(true);
		action->setChecked(tab.isAll ? currentTabId.isEmpty() : tab.id == currentTabId);
		const QString tabId = tab.id;
		const bool isAllTab = tab.isAll;
		connect(action, &QAction::triggered, this,
			[this, tile, tabId, isAllTab]() { assignSceneToTab(tile, tabId, isAllTab); });
	}

	menu.addSeparator();
	QAction *collapseAction =
		menu.addAction(tile->isCollapsed() ? T("Expand") : T("Collapse"));
	connect(collapseAction, &QAction::triggered, this, [tile]() { tile->setCollapsed(!tile->isCollapsed()); });

	QAction *chosen = menu.exec(globalPos);
	if (!chosen)
		return;

	if (chosen == renameAction)
		renameScene(tile);
	else if (chosen == duplicateAction)
		duplicateScene(tile);
	else if (chosen == removeAction) {
		obs_source_remove(source);
		rebuildLater();
	} else if (chosen == filtersAction)
		obs_frontend_open_source_filters(source);
	else if (chosen == propertiesAction)
		obs_frontend_open_source_properties(source);
}

void SceneWallWidget::renameScene(SceneTileWidget *tile)
{
	if (!tile || !tile->sceneSource())
		return;

	bool ok = false;
	const QString name =
		QInputDialog::getText(this, T("RenameSceneTitle"), T("SceneName"), QLineEdit::Normal,
				      tile->sceneName(), &ok);
	if (!ok || name.isEmpty())
		return;

	obs_source_set_name(tile->sceneSource(), name.toUtf8().constData());
	rebuildLater();
}

void SceneWallWidget::duplicateScene(SceneTileWidget *tile)
{
	if (!tile || !tile->sceneSource())
		return;

	obs_scene_t *scene = obs_scene_from_source(tile->sceneSource());
	if (!scene)
		return;

	QString name = tile->sceneName() + " copy";
	int suffix = 2;
	obs_source_t *existing = obs_get_source_by_name(name.toUtf8().constData());
	while (existing) {
		obs_source_release(existing);
		name = tile->sceneName() + " copy " + QString::number(suffix++);
		existing = obs_get_source_by_name(name.toUtf8().constData());
	}

	obs_scene_t *duplicate = obs_scene_duplicate(scene, name.toUtf8().constData(), OBS_SCENE_DUP_COPY);
	if (!duplicate)
		return;

	obs_source_t *duplicateSource = obs_scene_get_source(duplicate);
	if (duplicateSource)
		obs_frontend_set_current_scene(duplicateSource);
	obs_scene_release(duplicate);

	rebuildLater();
}

void SceneWallWidget::assignSceneToTab(SceneTileWidget *tile, const QString &tabId, bool isAllTab)
{
	if (!tile)
		return;

	SceneWallConfigData config = loadSceneWallConfig();
	const QString uuid = tile->sceneUuid();

	if (isAllTab)
		config.assignments.remove(uuid);
	else
		config.assignments.insert(uuid, tabId);

	saveSceneWallConfig(config);
	rebuildLater();
}

void SceneWallWidget::rebuildLater()
{
	// Odłóż przebudowę, aby nie usuwać kafla w trakcie obsługi jego zdarzenia.
	QTimer::singleShot(0, this, [this]() { loadTabs(); });
}

void SceneWallWidget::openSettings()
{
	SettingsDialog dialog(this);
	if (dialog.exec() == QDialog::Accepted)
		loadTabs();
}
