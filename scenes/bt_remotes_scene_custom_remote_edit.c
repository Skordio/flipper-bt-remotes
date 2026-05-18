#include "../bt_remotes.h"

// Static label buffers — valid while this scene is the active scene.
static char s_edit_labels[CustomRemoteInputCount][48];

static void cr_edit_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void build_edit_submenu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->editing_remote.name);

    for(uint8_t i = 0; i < CustomRemoteInputCount; i++) {
        char stem[20];
        hid_custom_remote_stem(app->editing_remote.scripts[i], stem, sizeof(stem));
        snprintf(s_edit_labels[i], sizeof(s_edit_labels[i]),
                 "%s: %s", custom_remote_input_labels[i], stem);
        submenu_add_item(app->submenu, s_edit_labels[i], i, cr_edit_cb, app);
    }
}

void bt_remotes_scene_custom_remote_edit_on_enter(void* context) {
    Hid* app = context;
    build_edit_submenu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_custom_remote_edit_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event < CustomRemoteInputCount) {
        app->editing_remote_input_idx = (uint8_t)event.event;
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneCustomRemoteAssign);
        return true;
    }
    return false;
}

void bt_remotes_scene_custom_remote_edit_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
