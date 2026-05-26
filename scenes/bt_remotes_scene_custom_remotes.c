#include "../bt_remotes.h"

enum BtRemotesCustomRemotesIndex {
    BtRemotesCustomRemotesCreate = 0,
    BtRemotesCustomRemotesEdit,
    BtRemotesCustomRemotesActive,
    BtRemotesCustomRemotesDelete,
};

static void bt_remotes_scene_custom_remotes_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bt_remotes_scene_custom_remotes_on_enter(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Custom Remotes");
    submenu_add_item(
        app->submenu, "Create a Remote", BtRemotesCustomRemotesCreate,
        bt_remotes_scene_custom_remotes_cb, app);
    submenu_add_item(
        app->submenu, "Edit a Remote", BtRemotesCustomRemotesEdit,
        bt_remotes_scene_custom_remotes_cb, app);
    submenu_add_item(
        app->submenu, "Active Remotes", BtRemotesCustomRemotesActive,
        bt_remotes_scene_custom_remotes_cb, app);
    submenu_add_item(
        app->submenu, "Delete a Remote", BtRemotesCustomRemotesDelete,
        bt_remotes_scene_custom_remotes_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_custom_remotes_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case BtRemotesCustomRemotesCreate:
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCustomRemoteCreate);
            return true;
        case BtRemotesCustomRemotesEdit:
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCustomRemoteSelect);
            return true;
        case BtRemotesCustomRemotesActive:
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCustomRemoteActive);
            return true;
        case BtRemotesCustomRemotesDelete:
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCustomRemoteDelete);
            return true;
        default:
            break;
        }
    }
    return false;
}

void bt_remotes_scene_custom_remotes_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
