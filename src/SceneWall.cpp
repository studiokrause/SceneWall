#include "SceneWall.h"
#include "SettingsDialog.h"

#include <QDrag>
#include <QEvent>
#include <QFontMetrics>
#include <QList>
#include <QMap>
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
#include <util/c99defs.h>
#include <media-io/video-io.h>
#include <graphics/graphics.h>
#include <graphics/vec4.h>

/* ------------------------------------------------------------------ */
/* SceneThumbnailWidget                                                */
/* ------------------------------------------------------------------ */

SceneThumbnailWidget::SceneThumbnailWidget(obs_source_t *src, QWidget *parent)
    : QWidget(parent), source(obs_source_get_ref(src))
{
    applySize();

    // Each thumbnail owns its own timer so that every one renders at the
    // requested rate. A shared round-robin scheduler was tried and reverted:
    // rendering one thumbnail per tick meant each one only updated every
    // N * tick, which visibly stuttered in Realtime mode.
    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &SceneThumbnailWidget::updateThumbnail);
    refreshTimer->start(500); // 2 FPS
}

SceneThumbnailWidget::~SceneThumbnailWidget()
{
    refreshTimer->stop();

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

    obs_source_release(source);
}

QString SceneThumbnailWidget::sceneName() const
{
    return QString(obs_source_get_name(source));
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

void SceneThumbnailWidget::setRefreshInterval(int ms)
{
    refreshTimer->setInterval(ms);
}

void SceneThumbnailWidget::startTimer()
{
    if (!isCollapsed)
        refreshTimer->start();
}

void SceneThumbnailWidget::stopTimer()
{
    refreshTimer->stop();
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

void SceneThumbnailWidget::updateThumbnail()
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
    if (isCollapsed) {
        thumbnail = QImage(); // only the bar remains
        refreshTimer->stop();
    } else {
        refreshTimer->start();
    }
    applySize();
    update();

    emit collapseToggled(sceneName(), isCollapsed);

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
        // Deep-copy into `thumbnail` BEFORE releasing the frame: `img` only
        // wraps frame->data, which obs_source_release_frame may free.
        thumbnail = img.copy().scaled(previewSize(), Qt::KeepAspectRatio,
                                      Qt::SmoothTransformation);
        ok = !thumbnail.isNull();
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
        gs_set_viewport(0, 0, (int)tw, (int)th);
        gs_clear(GS_CLEAR_COLOR, &clearColor, 0.0f, 0);
        // Map the whole scene into the thumbnail-sized viewport.
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

    // Program / Preview frames. These come from flags cached by
    // SceneWallWidget, so no OBS frontend API call happens during painting.
    if (isProgram) {
        painter.setPen(QPen(QColor(255, 0, 0), 3));
        painter.drawRect(0, 0, width() - 1, height() - 1);
    } else if (isPreview) {
        painter.setPen(QPen(QColor(0, 255, 0), 3));
        painter.drawRect(0, 0, width() - 1, height() - 1);
    }
}

void SceneThumbnailWidget::mousePressEvent(QMouseEvent *event)
{
    const QPoint pos = event->position().toPoint();
    dragStartPos = pos;

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

    if (event->button() == Qt::LeftButton)
        obs_frontend_set_current_scene(source);
}

void SceneThumbnailWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (!(event->buttons() & Qt::LeftButton))
        return;
    if ((event->position().toPoint() - dragStartPos).manhattanLength() < 5)
        return;

    // Left-drag reorders scenes inside the tab.
    QDrag *drag = new QDrag(this);
    QMimeData *mime = new QMimeData;
    mime->setData(SceneContainer::MIME, sceneName().toUtf8());
    drag->setMimeData(mime);
    drag->setPixmap(grab());
    drag->exec(Qt::MoveAction);
    drag->deleteLater();
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

QString SceneWallWidget::currentTabId() const
{
    const int i = tabContainer->currentIndex();
    if (i < 0 || i >= config.tabs.size())
        return QString();
    return config.tabs[i].id;
}

void SceneWallWidget::refreshTabColors()
{
    tabContainer->sceneTabBar()->clearColors();
    for (int i = 0; i < config.tabs.size() && i < tabContainer->sceneTabBar()->count(); ++i)
        tabContainer->sceneTabBar()->setTabColor(i, config.tabs[i].color());
    tabContainer->sceneTabBar()->update();
}

void SceneWallWidget::syncWithObs(const QStringList &obsScenes)
{
    if (config.tabs.isEmpty()) {
        TabConfig all;
        all.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        all.isAll = true;
        all.colorHex = "#808080";
        config.tabs.append(all);
    }

    for (TabConfig &t : config.tabs) {
        // Drop scenes that no longer exist in OBS.
        for (int i = t.scenes.size() - 1; i >= 0; --i)
            if (!obsScenes.contains(t.scenes[i]))
                t.scenes.removeAt(i);

        // The "All" tab always mirrors the full OBS scene list.
        if (t.isAll)
            for (const QString &s : obsScenes)
                if (!t.scenes.contains(s))
                    t.scenes.append(s);
    }
}

void SceneWallWidget::loadTabs()
{
    const int previousIndex = tabContainer->currentIndex();

    tabContainer->blockSignals(true);
    tabContainer->clear();
    tabContainer->sceneTabBar()->clearColors();

    // Old widgets are gone (clear() deleted them) - drop stale pointers.
    m_thumbWidgets.clear();

    struct obs_frontend_source_list scenes = {};
    obs_frontend_get_scenes(&scenes);

    QStringList obsScenes;
    QMap<QString, obs_source_t *> byName;
    for (size_t i = 0; i < scenes.sources.num; i++) {
        obs_source_t *src = scenes.sources.array[i];
        const QString n = obs_source_get_name(src);
        obsScenes.append(n);
        byName.insert(n, src);
    }

    syncWithObs(obsScenes);

    const int interval = config.realtime ? realtimeIntervalMs() : 500;

    // A scene's bar takes the colour of the tab it is assigned to, so the
    // owner stays recognisable even while browsing the "All" tab.
    QMap<QString, QColor> ownerColor;
    QColor allColor(128, 128, 128);
    for (const TabConfig &t : config.tabs)
        if (t.isAll)
            allColor = t.color();
    for (const TabConfig &t : config.tabs) {
        if (t.isAll)
            continue;
        for (const QString &s : t.scenes)
            if (!ownerColor.contains(s))
                ownerColor.insert(s, t.color());
    }

    for (int i = 0; i < config.tabs.size(); ++i) {
        const TabConfig &tab = config.tabs[i];
        const QColor color = tab.color();

        QScrollArea *scroll = new QScrollArea();
        scroll->setWidgetResizable(true);

        SceneContainer *container = new SceneContainer();
        connect(container, &SceneContainer::sceneDropped, this,
                &SceneWallWidget::onSceneDropped);

        for (const QString &name : tab.scenes) {
            if (!byName.contains(name))
                continue;
            SceneThumbnailWidget *w = new SceneThumbnailWidget(byName.value(name), container);
            w->setThumbSize(config.thumbSize);
            w->setBarColor(ownerColor.value(name, allColor));
            w->setRefreshInterval(interval);
            connect(w, &SceneThumbnailWidget::menuRequested, this, &SceneWallWidget::onSceneMenu);
            connect(w, &SceneThumbnailWidget::collapseToggled, this,
                    &SceneWallWidget::onCollapseToggled);
            container->flowLayout()->addWidget(w);
            m_thumbWidgets.append(w);
            // Keep bars collapsed across rebuilds (Settings save, assignment…).
            if (m_collapsedScenes.contains(name))
                w->setCollapsed(true);
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
    saveWallConfig(config);
}

void SceneWallWidget::applyThumbSize(int size)
{
    config.thumbSize = size;
    // One size for every tab; Autosize is only *computed* from the current tab.
    for (SceneThumbnailWidget *w : m_thumbWidgets)
        w->setThumbSize(size);
    saveWallConfig(config);

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

void SceneWallWidget::onCollapseToggled(const QString &sceneName, bool collapsed)
{
    if (collapsed)
        m_collapsedScenes.insert(sceneName);
    else
        m_collapsedScenes.remove(sceneName);
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

    const QList<SceneThumbnailWidget *> widgets =
            container->findChildren<SceneThumbnailWidget *>();
    if (widgets.isEmpty())
        return config.thumbSize;

    const int MIN = sizeSlider->minimum();
    const int MAX = sizeSlider->maximum();
    const int spacing = 6;
    const int usableW = avail.width() - 12;

    for (int s = MAX; s >= MIN; --s) {
        const int itemH = SceneThumbnailWidget::HEADER_HEIGHT + s * 9 / 16;

        int rows = 1;
        int rowW = 0;
        for (SceneThumbnailWidget *w : widgets) {
            // A collapsed bar is much narrower than a full thumbnail.
            const int itemW = w->collapsed() ? SceneThumbnailWidget::COLLAPSED_WIDTH : s;
            if (rowW > 0 && rowW + spacing + itemW > usableW) {
                rows++;
                rowW = 0;
            }
            rowW += (rowW > 0 ? spacing : 0) + itemW;
        }

        const int totalH = rows * itemH + (rows - 1) * spacing;
        if (totalH <= avail.height())
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
    if (from < 0 || to < 0 || from >= config.tabs.size() || to >= config.tabs.size())
        return;
    config.tabs.move(from, to);
    saveWallConfig(config);
    refreshTabColors();
}

void SceneWallWidget::onSceneDropped(const QString &sceneName, int index)
{
    moveSceneInTab(sceneName, index);
}

void SceneWallWidget::moveSceneInTab(const QString &sceneName, int index)
{
    const int ti = config.indexOfTab(currentTabId());
    if (ti < 0)
        return;

    QStringList &list = config.tabs[ti].scenes;
    const int from = list.indexOf(sceneName);
    if (from < 0)
        return;

    int target = index;
    if (from < target)
        target--; // the removed item shifts everything after it
    target = qBound(0, target, list.size() - 1);
    if (target == from)
        return;

    list.removeAt(from);
    list.insert(target, sceneName);

    saveWallConfig(config);
    loadTabs();
}

void SceneWallWidget::assignSceneToTab(const QString &sceneName, const QString &tabId)
{
    for (TabConfig &t : config.tabs) {
        if (t.isAll)
            continue;
        t.scenes.removeAll(sceneName);
    }

    const int ti = config.indexOfTab(tabId);
    if (ti >= 0 && !config.tabs[ti].isAll && !config.tabs[ti].scenes.contains(sceneName))
        config.tabs[ti].scenes.append(sceneName);

    saveWallConfig(config);
    loadTabs();
}

void SceneWallWidget::removeSceneFromTab(const QString &sceneName, const QString &tabId)
{
    const int ti = config.indexOfTab(tabId);
    if (ti >= 0 && !config.tabs[ti].isAll)
        config.tabs[ti].scenes.removeAll(sceneName);

    saveWallConfig(config);
    loadTabs();
}

void SceneWallWidget::onSceneMenu(SceneThumbnailWidget *widget, QPoint globalPos)
{
    if (!widget)
        return;

    const QString name = widget->sceneName();
    const QString tabId = currentTabId();
    const int ti = config.indexOfTab(tabId);

    QMenu menu;

    QMenu *assignMenu = menu.addMenu(obs_module_text("AssignToTab"));
    for (const TabConfig &t : config.tabs) {
        if (t.isAll)
            continue;
        QAction *a = assignMenu->addAction(t.name.isEmpty() ? t.id : t.name);
        a->setCheckable(true);
        a->setChecked(t.scenes.contains(name));
        // Clicking an already-assigned tab clears the assignment, so the
        // checkbox can be unchecked again.
        connect(a, &QAction::triggered, this, [this, name, id = t.id]() {
            const int ti = config.indexOfTab(id);
            if (ti >= 0 && config.tabs[ti].scenes.contains(name))
                removeSceneFromTab(name, id);
            else
                assignSceneToTab(name, id);
        });
    }

    if (ti >= 0 && !config.tabs[ti].isAll)
        menu.addAction(obs_module_text("RemoveFromTab"),
                       [this, name, tabId]() { removeSceneFromTab(name, tabId); });

    menu.addSeparator();

    menu.addAction(widget->collapsed() ? obs_module_text("Expand") : obs_module_text("Collapse"),
                   [widget]() { widget->setCollapsed(!widget->collapsed()); });

    menu.addAction(obs_module_text("Properties"),
                   [widget]() { obs_frontend_open_source_properties(widget->sourcePointer()); });

    menu.exec(globalPos);
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
    saveWallConfig(config);

    const int ms = on ? realtimeIntervalMs() : 500;
    for (SceneThumbnailWidget *w : m_thumbWidgets)
        w->setRefreshInterval(ms);
}

int SceneWallWidget::realtimeIntervalMs() const
{
    // Match the frame rate OBS is currently running at.
    const double fps = obs_get_active_fps();
    if (fps <= 0.0)
        return 33;
    return qMax(1, (int)qRound(1000.0 / fps));
}

void SceneWallWidget::setTimersRunning(bool running)
{
    for (SceneThumbnailWidget *w : m_thumbWidgets) {
        if (running)
            w->startTimer();
        else
            w->stopTimer();
    }
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
    setTimersRunning(true);
}

void SceneWallWidget::hideEvent(QHideEvent *event)
{
    setTimersRunning(false);
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
