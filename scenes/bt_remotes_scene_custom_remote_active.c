#include "../bt_remotes.h"

// Static label buffers — valid while this scene is active.
static char s_active_labels[BT_REMOTES_CUSTOM_REMOTE_MAX][BT_REMOTES_CUSTOM_REMOTE_NAME_LEN + 8];

static void cr_active_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// Returns true if 'name' is currently in app->active_custom_remotes[].
static bool is_active(Hid* app, const char* name) {
    for(uint8_t i = 0; i < app->active_custom_remote_count; i++) {
        if(strcmp(app->active_custom_remotes[i], name) == 0) return true;
    }
    return false;
}

static void build_active_submenu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Active Remotes");

    if(app->custom_remote_count == 0) {
        submenu_add_item(app->submenu, "(No remotes)", 0xFF, NULL, NULL);
        return;
    }

    for(uint8_t i = 0; i < app->custom_remote_count; i++) {
        snprintf(
            s_active_labels[i],
            sizeof(s_active_labels[i]),
            "%s %s",
            is_active(app, app->custom_remote_names[i]) ? "[ON]" : "[OFF]",
            app->custom_remote_names[i]);
        submenu_add_item(app->submenu, s_active_labels[i], i, cr_active_cb, app);
    }
}

void bt_remotes_scene_custom_remote_active_on_enter(void* context) {
    Hid* app = context;
    bt_remotes_custom_remote_load_list(app);
    build_active_submenu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_custom_remote_active_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event < app->custom_remote_count) {
        const char* name = app->custom_remote_names[event.event];

        // Toggle active state
        bool currently_active = is_active(app, name);
        if(currently_active) {
            // Remove from active list
            for(uint8_t i = 0; i < app->active_custom_remote_count; i++) {
                if(strcmp(app->active_custom_remotes[i], name) == 0) {
                    for(uint8_t j = i; j + 1 < app->active_custom_remote_count; j++) {
                        strlcpy(
                            app->active_custom_remotes[j],
                            app->active_custom_remotes[j + 1],
                            BT_REMOTES_CUSTOM_REMOTE_NAME_LEN);
                    }
                    app->active_custom_remote_count--;
                    break;
                }
            }
        } else {
            // Add to active list (if not at capacity)
            if(app->active_custom_remote_count < BT_REMOTES_CUSTOM_REMOTE_MAX) {
                strlcpy(
                    app->active_custom_remotes[app->active_custom_remote_count],
                    name,
                    BT_REMOTES_CUSTOM_REMOTE_NAME_LEN);
                app->active_custom_remote_count++;
            }
        }

        bt_remotes_active_remotes_save(app);

        // Rebuild submenu in-place to reflect the new state
        build_active_submenu(app);
        return true;
    }
    return false;
}

void bt_remotes_scene_custom_remote_active_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
