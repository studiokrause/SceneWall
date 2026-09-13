#include <obs-module.h>
#include <obs-frontend-api.h>
#include "SceneWall.h"
#include <QPointer>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("SceneWall", "en-US")

QPointer<SceneWallWidget> sceneWall;

static void create_dock() {
    sceneWall = new SceneWallWidget();
    obs_frontend_add_custom_qdock("SceneWall", sceneWall);
}

bool obs_module_load(void) {
    obs_frontend_add_event_callback([](enum obs_frontend_event event, void *ptr) {
        if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
            create_dock();
        }
    }, nullptr);
    return true;
}

void obs_module_unload(void) {
    if (sceneWall) {
        sceneWall->deleteLater();
    }
}
