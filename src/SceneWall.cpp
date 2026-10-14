#include "SceneWall.h"
#include "SettingsDialog.h"

#include <QDrag>
#include <QApplication>
#include <QEvent>
#include <QFontMetrics>
#include <QList>
#include <QMap>
#include <QInputDialog>
#include <QMenu>
#include <QMimeData>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QUuid>
#include <QVBoxLayout>
#include <QHBoxLayout>

#include <QPainter>
#include <QImage>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.h>
#include <obs-source.h>
#include <util/bmem.h>
#include <util/c99defs.h>
#include <media-io/video-io.h>
#include <graphics/graphics.h>
#include <graphics/vec4.h>

QString currentSceneCollectionKey()
{
    char *collection = obs_frontend_get_current_scene_collection();
    const QString key = collection ? QString::fromUtf8(collection) : QString();
    bfree(collection);
    return key;
}

WallConfig loadWallConfig()
{
    return loadWallConfig(getCfgPath(), currentSceneCollectionKey());
}

/* ------------------------------------------------------------------ */
/* SceneThumbnailWidget                                                */
/* ------------------------------------------------------------------ */

SceneThumbnailWidget::SceneThumbnailWidget(obs_source_t *src, QWidget *parent)
    : QWidget(parent), source(obs_source_get_ref(src))
{
    // Port of native Multiview (frontend/components/Multiview.cpp):
    // Multiview calls obs_source_inc_showing() for every scene it shows so
    // game/screen-capture, browser etc. stay active and renderable even when
    // the scene is neither in Preview nor Program. Without this,
    // obs_source_video_render() below returns black for inactive scenes.
    obs_source_inc_showing(source);

    applySize();
    // No per-widget timer: the wall scheduler (one QTimer, round-robin,
    // maxGpuReadsPerTick) calls renderIfVisible() so N scenes never run
    // N independent full GPU readbacks per tick.
}

SceneThumbnailWidget::~SceneThumbnailWidget()
{
    if (texrender || stagesurf) {
        obs_enter_graphics();
        if (stagesurf)
            gs_stagesurface_destroy(stagesurf);
        if (texrender)
            gs_texrender_destroy(texrender);
        obs_leave_graphics();
        stagesurf = nullptr;
        texrender = nullptr;
    }

    // Balance the inc_showing from the constructor (see Multiview::~Multiview).
    obs_source_dec_showing(source);
    obs_source_release(source);
}

QString SceneThumbnailWidget::sceneName() const
{
    return QString(obs_source_get_name(source));
}

QString SceneThumbnailWidget::sceneUuid() const
{
    return QString(obs_source_get_uuid(source));
}

void SceneThumbnailWidget::setThumbSize(int size)
{
    thumbSize = size;
    applySize();
    thumbnail = QImage(); // force re-render at the new resolution
    update();
}

void SceneThumbnailWidget::setBarColor(const QColor &color)
{
    barColor = color.isValid() ? color : QColor(80, 80, 80);
    update();
}

void SceneThumbnailWidget::setProgram(bool on)
{
    if (isProgram != on) {
        isProgram = on;
        update();
    }
}

void SceneThumbnailWidget::setPreview(bool on)
{
    if (isPreview != on) {
        isPreview = on;
        update();
    }
}

void SceneThumbnailWidget::renderIfVisible()
{
    // Skip the expensive GPU work for anything the user cannot actually see:
    // hidden dock, another tab, or scrolled out of the viewport.
    if (!isVisibleInViewport())
        return;

    renderPreview();
    update();
}

QSize SceneThumbnailWidget::previewSize() const
{
    return QSize(thumbSize, thumbSize * 9 / 16);
}

QRect SceneThumbnailWidget::headerRect() const
{
    return QRect(0, 0, width(), HEADER_HEIGHT);
}

QRect SceneThumbnailWidget::previewRect() const
{
    return QRect(0, HEADER_HEIGHT, width(), height() - HEADER_HEIGHT);
}

void SceneThumbnailWidget::applySize()
{
    const int h = HEADER_HEIGHT + previewSize().height();
    if (isCollapsed)
        setFixedSize(COLLAPSED_WIDTH, h);
    else
        setFixedSize(previewSize().width(), h);
    updateGeometry();
}

QSize SceneThumbnailWidget::sizeHint() const
{
    return QSize(isCollapsed ? COLLAPSED_WIDTH : previewSize().width(),
                 HEADER_HEIGHT + previewSize().height());
}

void SceneThumbnailWidget::setCollapsed(bool collapsed)
{
    if (isCollapsed == collapsed)
        return;

    isCollapsed = collapsed;
    if (isCollapsed)
        thumbnail = QImage(); // only the bar remains
    applySize();
    update();

    emit collapseToggled(sceneUuid(), isCollapsed);

    // Re-wrap the flow layout around the new item size. Never call
    // adjustSize() here: it would resize the container to its sizeHint (one
    // item wide) and collapse the whole tab into a single column.
    if (QWidget *p = parentWidget()) {
        if (QLayout *l = p->layout()) {
            l->invalidate();
            l->activate();
        }
        p->updateGeometry();
    }
}

bool SceneThumbnailWidget::isVisibleInViewport() const
{
    if (isCollapsed || !isVisible())
        return false;

    QWidget *container = parentWidget();
    if (!container)
        return false;

    // SceneContainer is installed as the scroll area's widget, so its parent
    // is the scroll viewport that does the clipping.
    QWidget *viewport = container->parentWidget();
    if (!viewport)
        return false;

    const QRect viewportRect(QPoint(0, 0), viewport->size());
    const QRect selfRect(container->mapTo(viewport, pos()), size());
    return selfRect.intersects(viewportRect);
}

void SceneThumbnailWidget::renderPreview()
{
    if (isCollapsed)
        return;
    if (previewSize().isEmpty())
        return;

    // Async sources (media etc.) expose a CPU frame; scenes do not, so they
    // fall through to the offscreen GPU render below.
    if (!renderAsyncFrame())
        renderSceneOffscreen();
}

bool SceneThumbnailWidget::renderAsyncFrame()
{
    struct obs_source_frame *frame = obs_source_get_frame(source);
    if (!frame)
        return false;

    QImage img;
    // OBS stores BGRA/RGBA packed; Qt's ARGB32 is BGRA in memory on little
    // endian, so no channel swap is needed for either case.
    if (frame->format == VIDEO_FORMAT_BGRA)
        img = QImage(frame->data[0], frame->width, frame->height, frame->linesize[0],
                     QImage::Format_ARGB32);
    else if (frame->format == VIDEO_FORMAT_RGBA)
        img = QImage(frame->data[0], frame->width, frame->height, frame->linesize[0],
                     QImage::Format_RGBA8888);

    bool ok = false;
    if (!img.isNull()) {
        // Deep-copy BEFORE releasing the frame: `img` only wraps
        // frame->data, which obs_source_release_frame may free. Keep the
        // layout box fixed (previewSize, 16:9) and letterbox: scale with
        // KeepAspectRatio, center on a black canvas, never stretch.
        const QSize box = previewSize();
        QImage scaled = img.copy().scaled(box, Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation);
        if (!scaled.isNull()) {
            thumbnail = QImage(box, QImage::Format_ARGB32);
            thumbnail.fill(Qt::black);
            QPainter painter(&thumbnail);
            painter.drawImage((box.width() - scaled.width()) / 2,
                              (box.height() - scaled.height()) / 2, scaled);
            ok = !thumbnail.isNull();
        }
    }

    // Safe to release only after the deep copy above has completed.
    obs_source_release_frame(source, frame);
    return ok;
}

bool SceneThumbnailWidget::renderSceneOffscreen()
{
    uint32_t sw = obs_source_get_width(source);
    uint32_t sh = obs_source_get_height(source);
    if (!sw || !sh) {
        sw = obs_source_get_base_width(source);
        sh = obs_source_get_base_height(source);
    }
    if (!sw || !sh)
        return false;

    const uint32_t tw = (uint32_t)previewSize().width();
    const uint32_t th = (uint32_t)previewSize().height();
    if (!tw || !th)
        return false;

    bool ok = false;

    obs_enter_graphics();

    // Allocate the GPU scratch buffers once and keep them; creating and
    // destroying them on every frame was a significant per-render cost.
    if (!texrender) {
        texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);
        if (!texrender) {
            obs_leave_graphics();
            return false;
        }
    }

    if (!stagesurf || stageW != (int)tw || stageH != (int)th) {
        if (stagesurf)
            gs_stagesurface_destroy(stagesurf);
        stagesurf = gs_stagesurface_create(tw, th, GS_BGRA);
        stageW = (int)tw;
        stageH = (int)th;
    }

    if (!stagesurf) {
        gs_texrender_destroy(texrender);
        texrender = nullptr;
        obs_leave_graphics();
        return false;
    }

    gs_texrender_reset(texrender);

    if (gs_texrender_begin(texrender, tw, th)) {
        struct vec4 clearColor;
        vec4_zero(&clearColor);
        // Black canvas first, then letterbox: viewport only on the image
        // rect so non-16:9 scenes (4:3, portrait) are not stretched.
        gs_set_viewport(0, 0, (int)tw, (int)th);
        gs_clear(GS_CLEAR_COLOR, &clearColor, 0.0f, 0);
        const float scale = qMin((float)tw / (float)sw, (float)th / (float)sh);
        const int dw = qMax(1, (int)(sw * scale));
        const int dh = qMax(1, (int)(sh * scale));
        gs_set_viewport(((int)tw - dw) / 2, ((int)th - dh) / 2, dw, dh);
        gs_ortho(0.0f, (float)sw, 0.0f, (float)sh, -100.0f, 100.0f);

        obs_source_video_render(source);
        gs_texrender_end(texrender);

        gs_texture_t *tex = gs_texrender_get_texture(texrender);
        if (tex) {
            gs_stage_texture(stagesurf, tex);
            uint8_t *data = nullptr;
            uint32_t linesize = 0;
            if (gs_stagesurface_map(stagesurf, &data, &linesize) && data && linesize) {
                QImage img(data, (int)tw, (int)th, (int)linesize, QImage::Format_ARGB32);
                if (!img.isNull())
                    thumbnail = img.copy();
                ok = !thumbnail.isNull();
                gs_stagesurface_unmap(stagesurf);
            }
        }
    }

    obs_leave_graphics();

    return ok;
}

void SceneThumbnailWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    const QString name = sceneName();

    if (isCollapsed) {
        // Vertical bar only: coloured background, white text rotated 90deg.
        painter.fillRect(rect(), barColor);
        painter.setPen(Qt::white);
        painter.save();
        painter.translate(width() / 2.0, height() / 2.0);
        painter.rotate(-90);
        QRect textRect(-height() / 2, -COLLAPSED_WIDTH / 2, height(), COLLAPSED_WIDTH);
        painter.drawText(textRect, Qt::AlignCenter, name);
        painter.restore();
    } else {
        // Header bar: white text on the tab colour.
        const QRect header = headerRect();
        painter.fillRect(header, barColor);
        painter.setPen(Qt::white);
        QFontMetrics fm(font());
        painter.drawText(header, Qt::AlignCenter,
                         fm.elidedText(name, Qt::ElideRight, header.width() - 8));

        // Preview area.
        const QRect preview = previewRect();
        if (thumbnail.isNull())
            painter.fillRect(preview, QColor(20, 20, 20));
        else
            painter.drawImage(preview.topLeft(), thumbnail);
    }

    // Frame around every thumbnail: thin grey by default, thicker red in
    // Program and thicker green in Preview, so the active scene stands out.
    // The colours come from flags cached by SceneWallWidget, so no OBS
    // frontend API call happens during painting.
    QColor frameColor(64, 64, 64);
    int frameWidth = 1;
    if (isProgram) {
        frameColor = QColor(255, 0, 0);
        frameWidth = 3;
    } else if (isPreview) {
        frameColor = QColor(0, 255, 0);
        frameWidth = 3;
    }

    painter.setPen(QPen(frameColor, frameWidth));
    painter.drawRect(0, 0, width() - 1, height() - 1);
}

void SceneThumbnailWidget::mousePressEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    dragStartPos = pos;
    dragStarted = false;

    // CTRL+click opens the plugin menu (never the right button).
    if (event->modifiers() & Qt::ControlModifier) {
        emit menuRequested(this, event->globalPosition().toPoint());
        return;
    }

    // Right button on the header (or anywhere on a collapsed bar) toggles
    // between the full thumbnail and the vertical bar. No menu here.
    if (event->button() == Qt::RightButton) {
        if (isCollapsed || headerRect().contains(pos)) {
            setCollapsed(!isCollapsed);
            return;
        }
        if (obs_frontend_preview_program_mode_active()) {
            obs_frontend_set_current_preview_scene(source);
            return;
        }
        return;
    }

    // Left press only arms a potential drag; Program switches on release.
}

void SceneThumbnailWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton))
        return;
    if ((event->position().toPoint() - dragStartPos).manhattanLength() <
        QApplication::startDragDistance())
        return;

    // Left-drag reorders scenes inside the tab (stable UUID, not the name,
    // so duplicate scene names reorder the right item).
    dragStarted = true;
    QDrag *drag = new QDrag(this);
    QMimeData *mime = new QMimeData;
    mime->setData(SceneContainer::MIME, sceneUuid().toUtf8());
    drag->setMimeData(mime);
    drag->setPixmap(grab());
    drag->exec(Qt::MoveAction);
    drag->deleteLater();
}

void SceneThumbnailWidget::mouseReleaseEvent(QMouseEvent *event)
{
    // Press already handled right button and CTRL+click; release only
    // switches Program for a plain left click that never became a drag.
    if (event->button() != Qt::LeftButton)
        return;
    if (event->modifiers() & Qt::ControlModifier)
        return;
    if (dragStarted)
        return;

    obs_frontend_set_current_scene(source);
}

/* ------------------------------------------------------------------ */
/* SceneWallWidget                                                     */
/* ------------------------------------------------------------------ */

// Drawn programmatically so the gear always renders, whatever the font covers.
static QIcon makeGearIcon(const QColor &color, int size = 18)
{
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(color);

    const qreal cx = size / 2.0;
    const qreal cy = size / 2.0;
    const qreal rOuter = size * 0.40;
    const qreal rInner = size * 0.17;
    const int teeth = 8;

    p.save();
    p.translate(cx, cy);
    for (int i = 0; i < teeth; ++i) {
        p.save();
        p.rotate(360.0 * i / teeth);
        p.drawRoundedRect(
                QRectF(-size * 0.055, -rOuter - size * 0.05, size * 0.11, size * 0.13), 1.0, 1.0);
        p.restore();
    }
    p.restore();

    // Ring with a hole (odd-even fill is the default for QPainterPath).
    QPainterPath path;
    path.addEllipse(QPointF(cx, cy), rOuter, rOuter);
    path.addEllipse(QPointF(cx, cy), rInner, rInner);
    p.drawPath(path);

    return QIcon(pm);
}

SceneWallWidget::SceneWallWidget(QWidget *parent) : QDockWidget(parent)
{
    setWindowTitle("SceneWall");
    m_collectionId = currentSceneCollectionKey();
    config = loadWallConfig();

    QWidget *content = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(content);
    // Small gap above the tab row (which also carries the toolbar).
    layout->setContentsMargins(0, 6, 0, 0);

    tabContainer = new SceneTabWidget(this);

    sizeSlider = new QSlider(Qt::Horizontal, this);
    sizeSlider->setRange(80, 400);
    sizeSlider->setValue(config.thumbSize);
    sizeSlider->setFixedWidth(110);

    autosizeBtn = new QPushButton(obs_module_text("Autosize"), this);

    realtimeToggle = new ToggleSwitch(this);
    realtimeToggle->setChecked(config.realtime); // before connect: no warning on startup
    realtimeToggle->setToolTip(obs_module_text("Realtime"));

    settingsBtn = new QPushButton(this);
    settingsBtn->setIcon(makeGearIcon(palette().color(QPalette::ButtonText)));
    settingsBtn->setIconSize(QSize(16, 16));
    settingsBtn->setToolTip(obs_module_text("Settings"));
    settingsBtn->setFixedSize(28, 22);

    QWidget *toolbar = new QWidget(this);
    QHBoxLayout *tb = new QHBoxLayout(toolbar);
    tb->setContentsMargins(0, 0, 2, 0);
    tb->setSpacing(6);
    tb->addWidget(new QLabel(obs_module_text("Size"), toolbar));
    tb->addWidget(sizeSlider);
    tb->addWidget(autosizeBtn);
    tb->addWidget(new QLabel(obs_module_text("Realtime"), toolbar));
    tb->addWidget(realtimeToggle);
    tb->addWidget(settingsBtn);
    tabContainer->setCornerWidget(toolbar, Qt::TopRightCorner);

    layout->addWidget(tabContainer);
    setWidget(content);

    connect(sizeSlider, &QSlider::valueChanged, this, &SceneWallWidget::applyThumbSize);
    connect(autosizeBtn, &QPushButton::clicked, this, &SceneWallWidget::runAutosize);
    connect(realtimeToggle, &ToggleSwitch::toggled, this, &SceneWallWidget::onRealtimeToggled);
    connect(settingsBtn, &QPushButton::clicked, this, &SceneWallWidget::openSettings);
    connect(tabContainer->sceneTabBar(), &QTabBar::tabMoved, this,
            &SceneWallWidget::onTabMoved);

    // Rebuild when OBS's own scene list changes (add / remove / rename), so
    // every tab stays in sync without reopening the panel.
    obs_frontend_add_event_callback(onFrontendEvent, this);

    // One shared render timer for all thumbnails: each tick renders at most
    // maxGpuReadsPerTick visible widgets round-robin (see onSchedulerTick).
    m_renderTimer = new QTimer(this);
    connect(m_renderTimer, &QTimer::timeout, this, &SceneWallWidget::onSchedulerTick);

    loadTabs();
    updateIndicators();
}

SceneWallWidget::~SceneWallWidget()
{
    obs_frontend_remove_event_callback(onFrontendEvent, this);
}

void SceneWallWidget::onFrontendEvent(enum obs_frontend_event event, void *param)
{
    auto *wall = static_cast<SceneWallWidget *>(param);
    if (!wall)
        return;

    switch (event) {
    case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
    case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
        // Queued: never rebuild while OBS is still handling its own event.
        QMetaObject::invokeMethod(wall, &SceneWallWidget::reloadFromObs,
                                  Qt::QueuedConnection);
        break;
    case OBS_FRONTEND_EVENT_SCENE_CHANGED:
    case OBS_FRONTEND_EVENT_PREVIEW_SCENE_CHANGED:
    case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
    case OBS_FRONTEND_EVENT_STUDIO_MODE_DISABLED:
        // Only the Program/Preview highlight changed - no rebuild needed.
        QMetaObject::invokeMethod(wall, &SceneWallWidget::updateIndicators,
                                  Qt::QueuedConnection);
        break;
    default:
        break;
    }
}

void SceneWallWidget::updateIndicators()
{
    // Query the frontend once here and push the result down to every
    // thumbnail, instead of each one calling the API on every repaint.
    obs_source_t *current = obs_frontend_get_current_scene();
    obs_source_t *preview =
            obs_frontend_preview_program_mode_active()
                    ? obs_frontend_get_current_preview_scene()
                    : nullptr;

    for (SceneThumbnailWidget *w : m_thumbWidgets) {
        w->setProgram(current && w->sourcePointer() == current);
        w->setPreview(preview && w->sourcePointer() == preview);
    }

    if (current)
        obs_source_release(current);
    if (preview)
        obs_source_release(preview);
}

void SceneWallWidget::reloadFromObs()
{
    // loadTabs() re-runs syncWithObs(), so every tab picks up the new scene set.
    loadTabs();
}

bool SceneWallWidget::persistConfig()
{
    if (saveWallConfig(config))
        return true;
    QMessageBox::warning(this, tr("SceneWall"),
                         tr("Could not save the SceneWall configuration. "
                            "Your changes are kept for this session but will be lost on restart."));
    return false;
}

CollectionConfig &SceneWallWidget::currentCollection()
{
    return config.ensureCollection(m_collectionId);
}

QString SceneWallWidget::currentTabId() const
{
    const int i = tabContainer->currentIndex();
    const int ci = config.indexOfCollection(m_collectionId);
    if (ci < 0)
        return QString();
    const CollectionConfig &collection = config.collections[ci];
    if (i < 0 || i >= collection.tabs.size())
        return QString();
    return collection.tabs[i].id;
}

void SceneWallWidget::refreshTabColors()
{
    const int ci = config.indexOfCollection(m_collectionId);
    if (ci < 0)
        return;
    const CollectionConfig &collection = config.collections[ci];
    tabContainer->sceneTabBar()->clearColors();
    for (int i = 0; i < collection.tabs.size() && i < tabContainer->sceneTabBar()->count(); ++i)
        tabContainer->sceneTabBar()->setTabColor(i, collection.tabs[i].color());
    tabContainer->sceneTabBar()->update();
}

void SceneWallWidget::syncWithObs(const QStringList &obsSceneUuids,
                                  const QMap<QString, QStringList> &uuidsBySceneName)
{
    CollectionConfig &collection = currentCollection();
    if (collection.tabs.isEmpty()) {
        TabConfig all;
        all.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        all.isAll = true;
        all.colorHex = "#808080";
        collection.tabs.append(all);
    }

    migrateLegacySceneNames(collection, uuidsBySceneName);
    filterDeletedScenes(collection, QSet<QString>(obsSceneUuids.begin(), obsSceneUuids.end()));

    for (TabConfig &t : collection.tabs) {
        if (t.isAll)
            for (const QString &sceneUuid : obsSceneUuids)
                if (!t.scenes.contains(sceneUuid))
                    t.scenes.append(sceneUuid);
    }
}

void SceneWallWidget::loadTabs()
{
    m_collectionId = currentSceneCollectionKey();
    const int previousIndex = tabContainer->currentIndex();

    stopScheduler(); // never tick while widgets are being rebuilt
    tabContainer->blockSignals(true);
    // QTabWidget::clear() only removes pages without deleting them, so
    // destroy each page explicitly: deleting the scroll area deletes its
    // SceneContainer and every thumbnail (dec_showing runs in each dtor).
    while (tabContainer->count() > 0) {
        QWidget *page = tabContainer->widget(0);
        tabContainer->removeTab(0);
        delete page;
    }
    tabContainer->sceneTabBar()->clearColors();

    // Old widgets are gone (deleted above) - drop stale pointers.
    m_thumbWidgets.clear();

    struct obs_frontend_source_list scenes = {};
    obs_frontend_get_scenes(&scenes);

    QStringList obsSceneUuids;
    QMap<QString, obs_source_t *> sourcesByUuid;
    QMap<QString, QStringList> uuidsBySceneName;
    for (size_t i = 0; i < scenes.sources.num; i++) {
        obs_source_t *src = scenes.sources.array[i];
        const QString name = obs_source_get_name(src);
        const QString uuid = obs_source_get_uuid(src);
        obsSceneUuids.append(uuid);
        sourcesByUuid.insert(uuid, src);
        uuidsBySceneName[name].append(uuid);
    }

    syncWithObs(obsSceneUuids, uuidsBySceneName);
    CollectionConfig &collection = currentCollection();

    // A scene's bar takes the colour of the tab it is assigned to, so the
    // owner stays recognisable even while browsing the "All" tab.
    QMap<QString, QColor> ownerColor;
    QColor allColor(128, 128, 128);
    for (const TabConfig &t : collection.tabs)
        if (t.isAll)
            allColor = t.color();
    for (const TabConfig &t : collection.tabs) {
        if (t.isAll)
            continue;
        for (const QString &s : t.scenes)
            if (!ownerColor.contains(s))
                ownerColor.insert(s, t.color());
    }

    for (int i = 0; i < collection.tabs.size(); ++i) {
        const TabConfig &tab = collection.tabs[i];
        const QColor color = tab.color();

        QScrollArea *scroll = new QScrollArea();
        scroll->setWidgetResizable(true);

        SceneContainer *container = new SceneContainer();
        connect(container, &SceneContainer::sceneDropped, this,
                &SceneWallWidget::onSceneDropped);

        for (const QString &sceneUuid : tab.scenes) {
            if (!sourcesByUuid.contains(sceneUuid))
                continue;
            SceneThumbnailWidget *w = new SceneThumbnailWidget(sourcesByUuid.value(sceneUuid), container);
            w->setThumbSize(config.thumbSize);
            w->setBarColor(ownerColor.value(sceneUuid, allColor));
            connect(w, &SceneThumbnailWidget::menuRequested, this, &SceneWallWidget::onSceneMenu);
            connect(w, &SceneThumbnailWidget::collapseToggled, this,
                    &SceneWallWidget::onCollapseToggled);
            container->flowLayout()->addWidget(w);
            m_thumbWidgets.append(w);
            // Keep bars collapsed across rebuilds (Settings save, assignment…).
            if (collection.collapsedScenes.contains(sceneUuid)) {
                w->blockSignals(true);
                w->setCollapsed(true);
                w->blockSignals(false);
            }
        }

        scroll->setWidget(container);

        const QString title =
                (tab.isAll && tab.name.isEmpty()) ? QString(obs_module_text("All")) : tab.name;
        tabContainer->addTab(scroll, title);
        tabContainer->sceneTabBar()->setTabColor(i, color);
    }

    obs_frontend_source_list_free(&scenes);

    tabContainer->blockSignals(false);

    if (previousIndex >= 0 && previousIndex < tabContainer->count())
        tabContainer->setCurrentIndex(previousIndex);

    reflowAll();
    // An empty snapshot (OBS startup / collection switch) proves nothing:
    // never persist it over saved assignments.
    if (!obsSceneUuids.isEmpty())
        persistConfig();
    startScheduler(); // no-op while the dock is hidden
}

void SceneWallWidget::applyThumbSize(int size)
{
    config.thumbSize = size;
    // One size for every tab; Autosize is only *computed* from the current tab.
    for (SceneThumbnailWidget *w : m_thumbWidgets)
        w->setThumbSize(size);
    persistConfig();

    reflowAll();
}

void SceneWallWidget::reflowAll()
{
    // Force every tab to re-wrap at the current dock width.
    for (int i = 0; i < tabContainer->count(); ++i) {
        QScrollArea *scroll = qobject_cast<QScrollArea *>(tabContainer->widget(i));
        if (!scroll)
            continue;
        QWidget *inner = scroll->widget();
        if (!inner)
            continue;
        if (QLayout *l = inner->layout()) {
            l->invalidate();
            l->activate();
        }
        inner->updateGeometry();
        scroll->updateGeometry();
    }
}

void SceneWallWidget::onCollapseToggled(const QString &sceneUuid, bool collapsed)
{
    if (collapsed)
        currentCollection().collapsedScenes.insert(sceneUuid);
    else
        currentCollection().collapsedScenes.remove(sceneUuid);
    persistConfig();
}

int SceneWallWidget::computeAutosize() const
{
    QScrollArea *scroll = qobject_cast<QScrollArea *>(tabContainer->currentWidget());
    if (!scroll)
        return config.thumbSize;

    const QSize avail = scroll->viewport()->size();
    if (avail.width() <= 0 || avail.height() <= 0)
        return config.thumbSize;

    SceneContainer *container = qobject_cast<SceneContainer *>(scroll->widget());
    if (!container)
        return config.thumbSize;

    FlowLayout *layout = container->flowLayout();
    if (!layout || layout->count() == 0)
        return config.thumbSize;

    const int MIN = sizeSlider->minimum();
    const int MAX = sizeSlider->maximum();
    const int margin = 6;
    const int spacing = 6;
    const int usableW = avail.width() - 2 * margin;
    const int usableH = avail.height() - 2 * margin;

    for (int s = MAX; s >= MIN; --s) {
        const int itemH = SceneThumbnailWidget::HEADER_HEIGHT + s * 9 / 16;

        int rows = 1;
        int rowW = 0;
        bool fits = true;
        for (int i = 0; i < layout->count(); ++i) {
            QLayoutItem *item = layout->itemAt(i);
            SceneThumbnailWidget *w =
                    item ? qobject_cast<SceneThumbnailWidget *>(item->widget()) : nullptr;
            if (!w)
                continue;
            // A collapsed bar is much narrower than a full thumbnail.
            const int itemW = w->collapsed() ? SceneThumbnailWidget::COLLAPSED_WIDTH : s;
            if (itemW > usableW) {
                fits = false;
                break;
            }
            if (rowW > 0 && rowW + spacing + itemW > usableW) {
                rows++;
                rowW = 0;
            }
            rowW += (rowW > 0 ? spacing : 0) + itemW;
        }

        const int totalH = rows * itemH + (rows - 1) * spacing;
        if (fits && totalH <= usableH)
            return s;
    }

    return MIN;
}

void SceneWallWidget::runAutosize()
{
    const int size = computeAutosize();
    sizeSlider->setValue(size); // triggers applyThumbSize
}

void SceneWallWidget::onTabMoved(int from, int to)
{
    QList<TabConfig> &tabs = currentCollection().tabs;
    if (from < 0 || to < 0 || from >= tabs.size() || to >= tabs.size())
        return;
    tabs.move(from, to);
    persistConfig();
    refreshTabColors();
}

void SceneWallWidget::onSceneDropped(const QString &sceneUuid, int index)
{
    // UUID-only: an unknown UUID is rejected inside moveSceneInTab without
    // touching the config (fixes duplicate-name reorder).
    moveSceneInTab(sceneUuid, index);
}

void SceneWallWidget::moveSceneInTab(const QString &sceneUuid, int index)
{
    CollectionConfig &collection = currentCollection();
    const int ti = collection.indexOfTab(currentTabId());
    if (ti < 0)
        return;

    QStringList &list = collection.tabs[ti].scenes;
    const int from = list.indexOf(sceneUuid);
    if (from < 0)
        return;

    int target = index;
    if (from < target)
        target--; // the removed item shifts everything after it
    target = qBound(0, target, list.size() - 1);
    if (target == from)
        return;

    list.removeAt(from);
    list.insert(target, sceneUuid);

    persistConfig();

    QScrollArea *scroll = qobject_cast<QScrollArea *>(tabContainer->currentWidget());
    SceneContainer *container =
            scroll ? qobject_cast<SceneContainer *>(scroll->widget()) : nullptr;
    if (!container)
        return;

    FlowLayout *layout = container->flowLayout();
    QList<QLayoutItem *> items;
    QMap<QString, QLayoutItem *> itemsByUuid;
    while (layout->count() > 0) {
        QLayoutItem *item = layout->takeAt(0);
        items.append(item);
        if (SceneThumbnailWidget *widget = qobject_cast<SceneThumbnailWidget *>(item->widget()))
            itemsByUuid.insert(widget->sceneUuid(), item);
    }

    QList<QLayoutItem *> ordered;
    for (const QString &uuid : list) {
        if (QLayoutItem *item = itemsByUuid.value(uuid, nullptr)) {
            layout->addItem(item);
            ordered.append(item);
        }
    }
    for (QLayoutItem *item : items)
        if (!ordered.contains(item))
            layout->addItem(item);

    layout->invalidate();
    layout->activate();
    container->updateGeometry();
}

void SceneWallWidget::assignSceneToTab(const QString &sceneUuid, const QString &tabId)
{
    CollectionConfig &collection = currentCollection();
    for (TabConfig &t : collection.tabs) {
        if (t.isAll)
            continue;
        t.scenes.removeAll(sceneUuid);
    }

    const int ti = collection.indexOfTab(tabId);
    if (ti >= 0 && !collection.tabs[ti].isAll && !collection.tabs[ti].scenes.contains(sceneUuid))
        collection.tabs[ti].scenes.append(sceneUuid);

    persistConfig();
    loadTabs();
}

void SceneWallWidget::removeSceneFromTab(const QString &sceneUuid, const QString &tabId)
{
    CollectionConfig &collection = currentCollection();
    const int ti = collection.indexOfTab(tabId);
    if (ti >= 0 && !collection.tabs[ti].isAll)
        collection.tabs[ti].scenes.removeAll(sceneUuid);

    persistConfig();
    loadTabs();
}

void SceneWallWidget::onSceneMenu(SceneThumbnailWidget *widget, QPoint globalPos)
{
    if (!widget)
        return;

    const QString sceneUuid = widget->sceneUuid();
    const QString tabId = currentTabId();
    CollectionConfig &collection = currentCollection();
    const int ti = collection.indexOfTab(tabId);

    QMenu menu;

    QMenu *assignMenu = menu.addMenu(obs_module_text("AssignToTab"));
    for (const TabConfig &t : collection.tabs) {
        if (t.isAll)
            continue;
        QAction *a = assignMenu->addAction(t.name.isEmpty() ? t.id : t.name);
        a->setCheckable(true);
        a->setChecked(t.scenes.contains(sceneUuid));
        // Clicking an already-assigned tab clears the assignment, so the
        // checkbox can be unchecked again.
        connect(a, &QAction::triggered, this, [this, sceneUuid, id = t.id]() {
            const int ti = currentCollection().indexOfTab(id);
            if (ti >= 0 && currentCollection().tabs[ti].scenes.contains(sceneUuid))
                removeSceneFromTab(sceneUuid, id);
            else
                assignSceneToTab(sceneUuid, id);
        });
    }

    if (ti >= 0 && !collection.tabs[ti].isAll)
        menu.addAction(obs_module_text("RemoveFromTab"),
                       [this, sceneUuid, tabId]() { removeSceneFromTab(sceneUuid, tabId); });

    menu.addSeparator();

    menu.addAction(widget->collapsed() ? obs_module_text("Expand") : obs_module_text("Collapse"),
                   [widget]() { widget->setCollapsed(!widget->collapsed()); });

    menu.addAction(obs_module_text("Filters"),
                   [widget]() { obs_frontend_open_source_filters(widget->sourcePointer()); });

    menu.addAction(obs_module_text("Rename"),
                   [this, name = widget->sceneName()]() { renameScene(name); });

    menu.addSeparator();

    menu.addAction(obs_module_text("AddScene"), [this]() { addScene(); });

    menu.exec(globalPos);
}

void SceneWallWidget::renameScene(const QString &oldName)
{
    bool ok = false;
    const QString newName =
        QInputDialog::getText(this, obs_module_text("Rename"), obs_module_text("Name"),
                              QLineEdit::Normal, oldName, &ok)
            .trimmed();
    if (!ok || newName.isEmpty() || newName == oldName)
        return;

    obs_source_t *existing = obs_get_source_by_name(newName.toUtf8().constData());
    if (existing) {
        obs_source_release(existing);
        QMessageBox::warning(this, obs_module_text("Rename"), obs_module_text("NameExists"));
        return;
    }

    obs_source_t *source = obs_get_source_by_name(oldName.toUtf8().constData());
    if (!source)
        return;
    obs_source_set_name(source, newName.toUtf8().constData());
    obs_source_release(source);

    // Assignments, order and collapse state are keyed by stable scene UUID,
    // so a rename needs no config migration.
    loadTabs();
}

void SceneWallWidget::addScene()
{
    QString suggestion = "Scene";
    for (int n = 2;; ++n) {
        obs_source_t *existing = obs_get_source_by_name(suggestion.toUtf8().constData());
        if (!existing)
            break;
        obs_source_release(existing);
        suggestion = QString("Scene %1").arg(n);
    }

    bool ok = false;
    const QString name =
        QInputDialog::getText(this, obs_module_text("AddScene"), obs_module_text("Name"),
                              QLineEdit::Normal, suggestion, &ok)
            .trimmed();
    if (!ok || name.isEmpty())
        return;

    obs_source_t *existing = obs_get_source_by_name(name.toUtf8().constData());
    if (existing) {
        obs_source_release(existing);
        QMessageBox::warning(this, obs_module_text("AddScene"), obs_module_text("NameExists"));
        return;
    }

    obs_scene_t *scene = obs_scene_create(name.toUtf8().constData());
    if (scene)
        obs_scene_release(scene);
    loadTabs();
}

void SceneWallWidget::onRealtimeToggled(bool on)
{
    if (on) {
        const auto answer = QMessageBox::warning(this, obs_module_text("Realtime"),
                                                 obs_module_text("HighResourceWarning"),
                                                 QMessageBox::Yes | QMessageBox::No,
                                                 QMessageBox::No);
        if (answer != QMessageBox::Yes) {
            realtimeToggle->blockSignals(true);
            realtimeToggle->setChecked(false);
            realtimeToggle->blockSignals(false);
            return;
        }
    }

    config.realtime = on;
    persistConfig();

    // Apply the new rate to the shared timer; no modal here, the warning
    // above already ran for the manual toggle (startup sets checked silent).
    stopScheduler();
    startScheduler();
}

int SceneWallWidget::realtimeIntervalMs() const
{
    // Match the frame rate OBS is currently running at.
    return ThumbnailScheduler::realtimeIntervalForFps(obs_get_active_fps());
}

void SceneWallWidget::onSchedulerTick()
{
    if (m_thumbWidgets.isEmpty())
        return;

    if (config.realtime) {
        for (SceneThumbnailWidget *w : m_thumbWidgets)
            if (w && !w->collapsed() && w->isVisible())
                w->renderIfVisible();
        return;
    }

    // Cheap eligibility first: collapsed, hidden-tab and hidden-dock widgets
    // never consume GPU budget. The viewport clip is rechecked inside
    // renderIfVisible(), which performs the expensive work.
    QList<bool> eligible;
    eligible.reserve(m_thumbWidgets.size());
    for (SceneThumbnailWidget *w : m_thumbWidgets)
        eligible.append(w && !w->collapsed() && w->isVisible());

    const QList<int> picked = m_schedule.pickNext(eligible);
    for (int i : picked) {
        if (SceneThumbnailWidget *w = m_thumbWidgets.value(i, nullptr))
            w->renderIfVisible();
    }
}

void SceneWallWidget::startScheduler()
{
    if (!m_renderTimer || m_renderTimer->isActive() || m_thumbWidgets.isEmpty())
        return;
    if (!isVisible())
        return; // showEvent resumes; hidden dock renders nothing
    m_renderTimer->start(config.realtime ? realtimeIntervalMs()
                                         : ThumbnailScheduler::NORMAL_INTERVAL_MS);
}

void SceneWallWidget::stopScheduler()
{
    if (m_renderTimer)
        m_renderTimer->stop();
}

void SceneWallWidget::refreshGearIcon()
{
    if (settingsBtn)
        settingsBtn->setIcon(makeGearIcon(palette().color(QPalette::ButtonText)));
}

void SceneWallWidget::changeEvent(QEvent *event)
{
    QDockWidget::changeEvent(event);
    // Re-tint the gear when the OBS theme/palette changes.
    if (event && event->type() == QEvent::PaletteChange)
        refreshGearIcon();
}

void SceneWallWidget::showEvent(QShowEvent *event)
{
    QDockWidget::showEvent(event);
    startScheduler(); // resume round-robin where it stopped; no budget lost
}

void SceneWallWidget::hideEvent(QHideEvent *event)
{
    stopScheduler();
    QDockWidget::hideEvent(event);
}

void SceneWallWidget::openSettings()
{
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // The dialog has already written its own changes, so re-read them
        // instead of clobbering them with our stale in-memory copy.
        config = loadWallConfig();
        loadTabs();
    }
}
