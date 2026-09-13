#include "SceneTileWidget.h"

#include "SceneHeaderWidget.h"

#include <QMouseEvent>
#include <QPainter>
#include <QVBoxLayout>

namespace {
constexpr int kHeaderHeight = 20;
constexpr int kCollapsedWidth = 28;
} // namespace

SceneTileWidget::SceneTileWidget(obs_source_t *scene, QWidget *parent)
	: QWidget(parent), scene(obs_source_get_ref(scene))
{
	header = new SceneHeaderWidget(sceneName(), this);

	thumb = new SceneThumbnailWidget(this->scene, this);
	thumb->installEventFilter(this);

	tileLayout = new QVBoxLayout(this);
	tileLayout->setContentsMargins(0, 0, 0, 0);
	tileLayout->setSpacing(0);
	tileLayout->addWidget(header);
	tileLayout->addWidget(thumb);

	connect(header, &SceneHeaderWidget::collapseRequested, this, &SceneTileWidget::toggleCollapse);

	setFixedSize(thumbSize, expandedHeight());

	ThumbnailManager::instance().attach(this, this, this->scene);
	ThumbnailManager::instance().setSize(this, thumbSize, thumbSize * 9 / 16);
}

SceneTileWidget::~SceneTileWidget()
{
	ThumbnailManager::instance().detach(this);
	if (scene)
		obs_source_release(scene);
}

int SceneTileWidget::expandedHeight() const
{
	return kHeaderHeight + thumbSize * 9 / 16;
}

QString SceneTileWidget::sceneUuid() const
{
	if (!scene)
		return QString();
	return QString::fromUtf8(obs_source_get_uuid(scene));
}

QString SceneTileWidget::sceneName() const
{
	if (!scene)
		return QString();
	return QString::fromUtf8(obs_source_get_name(scene));
}

QSize SceneTileWidget::sizeHint() const
{
	if (collapsed)
		return QSize(kCollapsedWidth, expandedHeight());
	return QSize(thumbSize, expandedHeight());
}

QSize SceneTileWidget::minimumSizeHint() const
{
	return sizeHint();
}

void SceneTileWidget::setThumbnailImage(const QImage &image)
{
	thumb->setThumbnail(QPixmap::fromImage(image));
}

void SceneTileWidget::setThumbSize(int size)
{
	thumbSize = size;
	if (collapsed) {
		setFixedSize(kCollapsedWidth, expandedHeight());
		updateGeometry();
		update();
		return;
	}

	thumb->setThumbSize(size);
	setFixedSize(size, expandedHeight());
	ThumbnailManager::instance().setSize(this, size, size * 9 / 16);
	updateGeometry();
}

void SceneTileWidget::setHeaderColor(const QColor &color)
{
	barColor = color;
	header->setBackgroundColor(color);
	update();
}

void SceneTileWidget::setRealtime(bool enable)
{
	thumb->setRealtime(enable);
}

bool SceneTileWidget::isRealtime() const
{
	return thumb->realtime;
}

void SceneTileWidget::setCollapsed(bool value)
{
	if (collapsed == value)
		return;
	collapsed = value;
	applyCollapsedState();
}

void SceneTileWidget::toggleCollapse()
{
	setCollapsed(!collapsed);
}

void SceneTileWidget::applyCollapsedState()
{
	if (collapsed) {
		header->hide();
		thumb->hide();
		ThumbnailManager::instance().setPaused(this, true);
		setFixedSize(kCollapsedWidth, expandedHeight());
	} else {
		header->show();
		thumb->show();
		ThumbnailManager::instance().setPaused(this, false);
		thumb->setThumbSize(thumbSize);
		setFixedSize(thumbSize, expandedHeight());
		ThumbnailManager::instance().setSize(this, thumbSize, thumbSize * 9 / 16);
	}

	updateGeometry();
	update();
}

bool SceneTileWidget::eventFilter(QObject *watched, QEvent *event)
{
	if (watched == thumb && event->type() == QEvent::MouseButtonPress) {
		auto *mouseEvent = static_cast<QMouseEvent *>(event);
		if (mouseEvent->modifiers() & Qt::ControlModifier) {
			emit menuRequested(this, mouseEvent->globalPosition().toPoint());
			return true;
		}
	}

	return QWidget::eventFilter(watched, event);
}

void SceneTileWidget::mousePressEvent(QMouseEvent *event)
{
	if (collapsed && (event->button() == Qt::LeftButton || event->button() == Qt::RightButton)) {
		setCollapsed(false);
		event->accept();
		return;
	}

	QWidget::mousePressEvent(event);
}

void SceneTileWidget::paintEvent(QPaintEvent *)
{
	if (!collapsed)
		return;

	QPainter painter(this);
	painter.fillRect(rect(), barColor);
	painter.setPen(Qt::white);

	painter.save();
	painter.translate(width() / 2, height() / 2);
	painter.rotate(-90);
	const QRect textRect(-height() / 2, -10, height(), 20);
	painter.drawText(textRect, Qt::AlignCenter, sceneName());
	painter.restore();
}
