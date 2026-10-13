#include <obs-module.h>
#include <obs-frontend-api.h>
#include "SceneWall.h"
#include <QPointer>

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("SceneWall", "en-US")

QPointer<SceneWallWidget> sceneWall;

static void create_dock() {
    if (sceneWall)
        return; // FINISHED_LOADING can fire more than once
    sceneWall = new SceneWallWidget();
    obs_frontend_add_custom_qdock("SceneWall", sceneWall);
}

static void on_frontend_loaded(enum obs_frontend_event event, void *ptr)
{
    (void)ptr;
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING)
        create_dock();
}

bool obs_module_load(void) {
    obs_frontend_add_event_callback(on_frontend_loaded, nullptr);
    return true;
}

void obs_module_unload(void) {
    obs_frontend_remove_event_callback(on_frontend_loaded, nullptr);
    if (sceneWall) {
        sceneWall->deleteLater();
    }
}
