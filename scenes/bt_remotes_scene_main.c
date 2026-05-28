#include "../bt_remotes.h"
#include "../views.h"

void bt_remotes_scene_main_on_enter(void* context) {
    Hid* app = context;
    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneMain));
}

bool bt_remotes_scene_main_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

void bt_remotes_scene_main_on_exit(void* context) {
    Hid* app = context;
    UNUSED(app);
}
