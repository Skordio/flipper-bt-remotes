#include "../bt_remotes.h"

// Profile Management sub-scene under Profile Settings. Rename / Save / Delete the
// active profile. Each row routes to its existing dedicated scene.

enum BtRemotesProfileManagementIndex {
    BtRemotesProfileManagementIndexRename,
    BtRemotesProfileManagementIndexShortcut,
    BtRemotesProfileManagementIndexDelete,
    BtRemotesProfileManagementIndexHelp,
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
        "Create Shortcut",
        BtRemotesProfileManagementIndexShortcut,
        bt_remotes_scene_profile_management_cb,
        app);
    submenu_add_item(
        app->submenu,
        "Delete Profile",
        BtRemotesProfileManagementIndexDelete,
        bt_remotes_scene_profile_management_cb,
        app);
    submenu_add_item(
        app->submenu,
        "Help",
        BtRemotesProfileManagementIndexHelp,
        bt_remotes_scene_profile_management_cb,
        app);

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneProfileManagement));

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_profile_management_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == BtRemotesProfileManagementIndexRename) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileRenameFile);
    } else if(event.event == BtRemotesProfileManagementIndexShortcut) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileShortcutCreate);
    } else if(event.event == BtRemotesProfileManagementIndexDelete) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneDeleteProfile);
    } else if(event.event == BtRemotesProfileManagementIndexHelp) {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneRemoteSettingsHelp, RemoteSettingsHelpProfileMgmt);
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRemoteSettingsHelp);
    }
    return true;
}

void bt_remotes_scene_profile_management_on_exit(void* context) {
    Hid* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        BtRemotesSceneProfileManagement,
        submenu_get_selected_item(app->submenu));
    submenu_reset(app->submenu);
}
