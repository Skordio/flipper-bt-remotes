#include "../bt_remotes.h"

static void cr_select_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bt_remotes_scene_custom_remote_select_on_enter(void* context) {
    Hid* app = context;
    bt_remotes_custom_remote_load_list(app);

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Edit Remote");

    if(app->custom_remote_count == 0) {
        submenu_add_item(app->submenu, "(No remotes)", 0xFF, NULL, NULL);
    } else {
        for(uint8_t i = 0; i < app->custom_remote_count; i++) {
            submenu_add_item(
                app->submenu, app->custom_remote_names[i], i, cr_select_cb, app);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_custom_remote_select_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event < app->custom_remote_count) {
        bt_remotes_custom_remote_load(app, app->custom_remote_names[event.event]);
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneCustomRemoteEdit);
        return true;
    }
    return false;
}

void bt_remotes_scene_custom_remote_select_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
