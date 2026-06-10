#include "../bt_remotes.h"

// Profile Management sub-scene under Profile Settings. Rename / Save / Delete the
// active profile. Each row routes to its existing dedicated scene.

enum BtRemotesProfileManagementIndex {
    BtRemotesProfileManagementIndexRename,
    BtRemotesProfileManagementIndexDelete,
};

static void bt_remotes_scene_profile_management_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bt_remotes_scene_profile_management_on_enter(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Profile Management");

    submenu_add_item(
        app->submenu,
        "Rename Profile",
        BtRemotesProfileManagementIndexRename,
        bt_remotes_scene_profile_management_cb,
        app);
    submenu_add_item(
        app->submenu,
        "Delete Profile",
        BtRemotesProfileManagementIndexDelete,
        bt_remotes_scene_profile_management_cb,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_profile_management_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == BtRemotesProfileManagementIndexRename) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileRenameFile);
    } else if(event.event == BtRemotesProfileManagementIndexDelete) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneDeleteProfile);
    }
    return true;
}

void bt_remotes_scene_profile_management_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
