#include "../bt_remotes.h"

// Static label buffers — valid while this scene is the active scene.
static char s_edit_labels[CustomRemoteInputCount][48];

// Extract the filename stem from a full path into a buffer (truncated to out_size-1).
// Empty path → "-".
static void edit_stem(const char* path, char* out, size_t out_size) {
    if(!path || path[0] == '\0') {
        strlcpy(out, "-", out_size);
        return;
    }
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    strlcpy(out, base, out_size);
    char* dot = strrchr(out, '.');
    if(dot) *dot = '\0';
    if(out[0] == '\0') strlcpy(out, "-", out_size);
}

static void cr_edit_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void build_edit_submenu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->editing_remote.name);

    for(uint8_t i = 0; i < CustomRemoteInputCount; i++) {
        char stem[20];
        edit_stem(app->editing_remote.scripts[i], stem, sizeof(stem));
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
