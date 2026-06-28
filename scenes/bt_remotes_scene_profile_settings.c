#include "../bt_remotes.h"

// Per-profile settings hub, reached from the Start menu. Pure chooser: each row
// opens a focused sub-scene. App-wide settings live in BtRemotesSceneGlobalSettings.
//
// BLE policy: BLE stays up through the whole Settings sub-tree so the paired host
// doesn't drop while the user is tweaking knobs. The one action that needs to
// re-advertise (renaming the active profile's BT name) cycles BLE locally in
// scene_rename.c. Delay-connect profiles keep BLE off in Settings as before —
// the Start scene's delay-connect branch already excludes the Settings selection
// from starting BLE.

enum BtRemotesProfileSettingsIndex {
    BtRemotesProfileSettingsIndexConnection,
    BtRemotesProfileSettingsIndexMenuLayout,
    BtRemotesProfileSettingsIndexPerRemote,
    BtRemotesProfileSettingsIndexProfileMgmt,
};

static void bt_remotes_scene_profile_settings_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bt_remotes_scene_profile_settings_on_enter(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Profile Settings");

    submenu_add_item(
        app->submenu,
        "Connection",
        BtRemotesProfileSettingsIndexConnection,
        bt_remotes_scene_profile_settings_cb,
        app);
    submenu_add_item(
        app->submenu,
        "Start Menu Layout",
        BtRemotesProfileSettingsIndexMenuLayout,
        bt_remotes_scene_profile_settings_cb,
        app);
    submenu_add_item(
        app->submenu,
        "Per-Remote Settings",
        BtRemotesProfileSettingsIndexPerRemote,
        bt_remotes_scene_profile_settings_cb,
        app);
    submenu_add_item(
        app->submenu,
        "Profile Management",
        BtRemotesProfileSettingsIndexProfileMgmt,
        bt_remotes_scene_profile_settings_cb,
        app);

    // Restore cursor to wherever the user left it (saved in on_exit).
    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneProfileSettings));

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_profile_settings_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == BtRemotesProfileSettingsIndexConnection) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileConnection);
    } else if(event.event == BtRemotesProfileSettingsIndexMenuLayout) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileMenuLayout);
    } else if(event.event == BtRemotesProfileSettingsIndexPerRemote) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRemoteTypeSettings);
    } else if(event.event == BtRemotesProfileSettingsIndexProfileMgmt) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileManagement);
    }
    return true;
}

void bt_remotes_scene_profile_settings_on_exit(void* context) {
    Hid* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        BtRemotesSceneProfileSettings,
        submenu_get_selected_item(app->submenu));
    submenu_reset(app->submenu);
}
