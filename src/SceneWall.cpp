#include "SceneWall.h"
#include "SettingsDialog.h"
#include "AboutDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <obs-frontend-api.h>
#include <obs.h>
#include <obs-source.h>
#include <obs-audio-controls.h>
#include <QPainter>
#include <QImage>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QStandardPaths>
#include <QDir>

static void volmeter_callback(void *param, const float[MAX_AUDIO_CHANNELS],
                              const float peak[MAX_AUDIO_CHANNELS],
                              const float[MAX_AUDIO_CHANNELS])
{
    SceneThumbnailWidget *widget = static_cast<SceneThumbnailWidget*>(param);
    widget->audioLevel = peak[0];
    widget->update();
}

SceneThumbnailWidget::SceneThumbnailWidget(obs_source_t* src, QWidget *parent) 
    : QWidget(parent), source(obs_source_get_ref(src)) {
    setFixedSize(thumbSize, thumbSize * 9 / 16);
    
    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &SceneThumbnailWidget::updateThumbnail);
    refreshTimer->start(500); // 2 FPS
    
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, &SceneThumbnailWidget::showContextMenu);
}

SceneThumbnailWidget::~SceneThumbnailWidget() {
    if (volmeter) {
        obs_volmeter_detach_source(volmeter);
        obs_volmeter_destroy(volmeter);
    }
    obs_source_release(source);
}

void SceneThumbnailWidget::setRealtime(bool enable) {
    if (enable && !realtime) {
        QMessageBox::warning(this, "High Resource Usage", 
            "Realtime mode enables high-frequency thumbnail updates, which may significantly increase CPU/GPU usage. Continue?");
        refreshTimer->setInterval(33); // ~30 FPS
    } else if (!enable) {
        refreshTimer->setInterval(500); // 2 FPS
    }
    realtime = enable;
}

void SceneThumbnailWidget::setShowAudio(bool enable) {
    showAudio = enable;
    if (showAudio && !volmeter) {
        volmeter = obs_volmeter_create(OBS_FADER_IEC);
        obs_volmeter_attach_source(volmeter, source);
        obs_volmeter_add_callback(volmeter, volmeter_callback, this);
    } else if (!showAudio && volmeter) {
        obs_volmeter_detach_source(volmeter);
        obs_volmeter_destroy(volmeter);
        volmeter = nullptr;
    }
    update();
}

void SceneThumbnailWidget::setThumbSize(int size) {
    thumbSize = size;
    setFixedSize(thumbSize, thumbSize * 9 / 16);
    update();
}

void SceneThumbnailWidget::updateThumbnail() {
    struct obs_source_frame* frame = obs_source_get_frame(source);
    if (frame) {
        QImage img(frame->data[0], frame->width, frame->height, frame->linesize[0], QImage::Format_ARGB32);
        thumbnail = img.rgbSwapped().copy().scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        obs_source_release_frame(source, frame);
    }
    
    muted = obs_source_muted(source);
    update(); // Trigger paintEvent
}

void SceneThumbnailWidget::paintEvent(QPaintEvent *event) {
    QPainter painter(this);
    
    if (isCollapsed) {
        // Collapsed mode: vertical bar
        painter.fillRect(rect(), QColor(40, 40, 40));
        painter.setPen(Qt::white);
        painter.save();
        painter.translate(width()/2, height()/2);
        painter.rotate(-90);
        QRect textRect(-height()/2, -15, height(), 30);
        painter.drawText(textRect, Qt::AlignCenter, obs_source_get_name(source));
        painter.restore();
        return;
    }
    
    if (!thumbnail.isNull()) {
        painter.drawImage(0, 0, thumbnail);
    } else {
        painter.fillRect(rect(), Qt::black);
    }
    
    // Name overlay
    painter.setPen(Qt::white);
    painter.fillRect(0, height()-20, width(), 20, QColor(0, 0, 0, 160));
    painter.drawText(QRect(0, height()-20, width(), 20), Qt::AlignCenter, obs_source_get_name(source));
    
    // Audio bar
    if (showAudio) {
        int barWidth = 8;
        int barHeight = height() - 20;
        int x = 2;
        int y = 0;
        
        QColor barColor = muted ? Qt::gray : (obs_source_active(source) ? QColor(0, 200, 0) : Qt::gray);
        painter.fillRect(x, y, barWidth, barHeight, QColor(80, 80, 80));
        int levelHeight = barHeight * audioLevel;
        painter.fillRect(x, y + (barHeight - levelHeight), barWidth, levelHeight, barColor);
    }
    
    // Preview/Program indicators
    obs_source_t* currentScene = obs_frontend_get_current_scene();
    if (currentScene) {
        if (currentScene == source) {
            painter.setPen(QPen(QColor(255, 0, 0), 3));
            painter.drawRect(0, 0, width()-1, height()-1);
        }
        obs_source_release(currentScene);
    }
    
    bool studioMode = obs_frontend_preview_program_mode_active();
    if (studioMode) {
        obs_source_t* previewScene = obs_frontend_get_current_preview_scene();
        if (previewScene) {
            if (previewScene == source) {
                painter.setPen(QPen(QColor(0, 255, 0), 3));
                painter.drawRect(0, 0, width()-1, height()-1);
            }
            obs_source_release(previewScene);
        }
    }
}

void SceneThumbnailWidget::mousePressEvent(QMouseEvent *event) {
    bool studioMode = obs_frontend_preview_program_mode_active();

    // Check if click is on audio bar hitbox
    if (showAudio && event->position().x() <= 12) {
        if (event->modifiers() & Qt::ControlModifier || event->button() == Qt::LeftButton) {
            bool newMuted = !obs_source_muted(source);
            obs_source_set_muted(source, newMuted);
            update();
            return;
        }
    }

    if (event->modifiers() & Qt::ControlModifier) {
        // CTRL+Click: Show advanced management menu
        QMenu menu(this);
        
        QMenu *assignMenu = menu.addMenu("Assign to Tab");
        // TODO: populate with tab names
        
        menu.addAction("Toggle Static/Dynamic", [this]() {
            // TODO: toggle static preview mode
        });
        menu.addAction("Generate Preview", [this]() {
            // TODO: generate static preview
        });
        
        QAction *audioAction = menu.addAction("Show Audio");
        audioAction->setCheckable(true);
        audioAction->setChecked(showAudio);
        connect(audioAction, &QAction::toggled, this, &SceneThumbnailWidget::setShowAudio);
        
        menu.addAction("Toggle Collapse", [this]() {
            isCollapsed = !isCollapsed;
            if (isCollapsed) {
                setFixedSize(30, 90);
            } else {
                setFixedSize(thumbSize, thumbSize * 9 / 16);
            }
            update();
        });
        
        menu.exec(event->globalPos());
    } else if (event->button() == Qt::LeftButton) {
        obs_frontend_set_current_scene(source);
    } else if (event->button() == Qt::RightButton) {
        if (studioMode) {
            obs_frontend_set_current_preview_scene(source);
        } else {
            showContextMenu(event->pos());
        }
    }
}

void SceneThumbnailWidget::showContextMenu(const QPoint &pos) {
    QMenu menu(this);
    QAction* realtimeAction = menu.addAction("Realtime Mode");
    realtimeAction->setCheckable(true);
    realtimeAction->setChecked(realtime);
    connect(realtimeAction, &QAction::toggled, this, &SceneThumbnailWidget::setRealtime);
    
    QAction* audioAction = menu.addAction("Show Audio");
    audioAction->setCheckable(true);
    audioAction->setChecked(showAudio);
    connect(audioAction, &QAction::toggled, this, &SceneThumbnailWidget::setShowAudio);
    
    menu.addAction("Mute", [this]() {
        bool newMuted = !obs_source_muted(source);
        obs_source_set_muted(source, newMuted);
        update();
    });
    
    menu.addAction("Properties", [this]() { obs_frontend_open_source_properties(source); });
    menu.exec(mapToGlobal(pos));
}

SceneWallWidget::SceneWallWidget(QWidget *parent) : QDockWidget(parent) {
    setWindowTitle("SceneWall");
    
    QWidget *content = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(content);
    
    // Toolbar
    QHBoxLayout *toolbarLayout = new QHBoxLayout();
    sizeSlider = new QSlider(Qt::Horizontal, this);
    sizeSlider->setRange(50, 300);
    sizeSlider->setValue(160);
    settingsBtn = new QPushButton("Settings", this);
    aboutBtn = new QPushButton("About", this);
    toolbarLayout->addWidget(new QLabel("Size:"));
    toolbarLayout->addWidget(sizeSlider);
    toolbarLayout->addWidget(settingsBtn);
    toolbarLayout->addWidget(aboutBtn);
    layout->addLayout(toolbarLayout);
    
    // Tabs
    tabContainer = new QTabWidget(this);
    layout->addWidget(tabContainer);
    
    loadTabs();
    
    connect(settingsBtn, &QPushButton::clicked, this, &SceneWallWidget::openSettings);
    connect(aboutBtn, &QPushButton::clicked, this, [this]() {
        AboutDialog dialog(this);
        dialog.exec();
    });
    
    setWidget(content);
}

QString getCfgPath() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/obs-studio/plugin_config/SceneWall/";
    QDir().mkpath(path);
    return path + "tabs.json";
}

void SceneWallWidget::loadTabs() {
    tabContainer->clear();
    
    QFile file(getCfgPath());
    QJsonArray tabs;
    
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        tabs = QJsonDocument::fromJson(file.readAll()).array();
        file.close();
    } else {
        QJsonObject allTab;
        allTab["name"] = "All";
        allTab["color"] = "#808080";
        tabs.append(allTab);
    }

    for (const auto& val : tabs) {
        QJsonObject tabObj = val.toObject();
        
        QScrollArea *scroll = new QScrollArea();
        scroll->setWidgetResizable(true);
        QWidget *tab = new QWidget();
        QGridLayout *tabLayout = new QGridLayout(tab);
        tabLayout->setSpacing(4);
        
        struct obs_frontend_source_list scenes = {};
        obs_frontend_get_scenes(&scenes);
        int col = 0;
        int row = 0;
        int maxCols = 4;
        for (size_t i = 0; i < scenes.sources.num; i++) {
            SceneThumbnailWidget *w = new SceneThumbnailWidget(scenes.sources.array[i], tab);
            w->setThumbSize(sizeSlider->value());
            tabLayout->addWidget(w, row, col);
            col++;
            if (col >= maxCols) {
                col = 0;
                row++;
            }
        }
        obs_frontend_source_list_free(&scenes);
        
        scroll->setWidget(tab);
        tabContainer->addTab(scroll, tabObj["name"].toString());
    }
}

void SceneWallWidget::openSettings() {
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        loadTabs();
    }
}
