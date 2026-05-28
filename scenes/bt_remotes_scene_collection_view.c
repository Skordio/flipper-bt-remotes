#include "../bt_remotes.h"

static void collection_view_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// Submenu stores label pointers, not copies — needs a stable buffer per slot.
static char view_labels[BT_REMOTES_COLLECTION_SCRIPT_MAX][64];

void bt_remotes_scene_collection_view_on_enter(void* context) {
    Hid* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->editing_collection_name);

    for(uint8_t i = 0; i < app->editing_collection_script_count; i++) {
        const char* path = app->editing_collection_scripts[i];
        const char* slash = strrchr(path, '/');
        const char* name  = slash ? slash + 1 : path;
        strlcpy(view_labels[i], name, sizeof(view_labels[i]));
        // Strip extension
        char* dot = strrchr(view_labels[i], '.');
        if(dot) *dot = '\0';
        submenu_add_item(app->submenu, view_labels[i], i, collection_view_cb, app);
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
