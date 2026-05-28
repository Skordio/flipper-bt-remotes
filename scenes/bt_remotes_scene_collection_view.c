#include "../bt_remotes.h"

static void collection_view_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// Returns a pointer to the filename portion of path (after the last '/').
static const char* path_basename(const char* path) {
    const char* s = strrchr(path, '/');
    return s ? s + 1 : path;
}

void bt_remotes_scene_collection_view_on_enter(void* context) {
    Hid* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->editing_collection_name);

    for(uint8_t i = 0; i < app->editing_collection_script_count; i++) {
        // Point into the stored path string so no extra allocation is needed.
        // The scripts array lives for the app lifetime, so the pointer is stable.
        submenu_add_item(
            app->submenu,
            path_basename(app->editing_collection_scripts[i]),
            i,
            collection_view_cb,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_collection_view_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        uint32_t idx = event.event;
        if(idx < app->editing_collection_script_count) {
            strlcpy(
                app->pending_script_path,
                app->editing_collection_scripts[idx],
                sizeof(app->pending_script_path));
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCustomActionsRun);
            return true;
        }
    }

    return false;
}

void bt_remotes_scene_collection_view_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
