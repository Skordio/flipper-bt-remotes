#include "../bt_remotes.h"
#include "../views.h"

void bt_remotes_scene_main_on_enter(void* context) {
    Hid* app = context;
    uint32_t view_id = scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneMain);
    // Normally a no-op (the Start scene ensures before configuring the view);
    // kept as a safety net for any path that enters Main directly.
    bt_remotes_view_ensure(app, view_id);
    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
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
