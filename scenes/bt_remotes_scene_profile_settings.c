#include "../bt_remotes.h"

// Per-profile settings hub, reached from the Start menu. Pure chooser: each row
// opens a focused sub-scene. App-wide settings live in BtRemotesSceneGlobalSettings.
//
// BLE policy: Start stops BLE when entering Settings (start.c:BtRemotesStartIndexSettings).
// On Back to Start, this scene restarts BLE for non-delay-connect profiles so the
// session reconnects automatically. Navigating within the Settings sub-tree keeps
// BLE stopped — only the back-to-Start transition triggers the restart.

enum BtRemotesProfileSettingsIndex {
    BtRemotesProfileSettingsIndexConnection,
    BtRemotesProfileSettingsIndexMenuLayout,
    BtRemotesProfileSettingsIndexPerRemote,
    BtRemotesProfileSettingsIndexProfileMgmt,
    BtRemotesProfileSettingsIndexHelp,
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
    submenu_add_item(
        app->submenu,
        "Help",
        BtRemotesProfileSettingsIndexHelp,
        bt_remotes_scene_profile_settings_cb,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_profile_settings_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        // Leaving Settings back to Start: restore BLE for non-delay-connect profiles.
        // Start stopped BLE on the way in (start.c). Delay-connect profiles stay
        // disconnected at the menu and Start re-enters its own BLE policy, so we
        // skip both activate and start there (matches the old settings scene).
        if(!app->ble_started && app->active_profile[0] != '\0' && !app->delay_connect) {
            bt_remotes_profile_activate(app);
            bt_remotes_start_ble(app);
        }
        return false;
    }

    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == BtRemotesProfileSettingsIndexConnection) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileConnection);
    } else if(event.event == BtRemotesProfileSettingsIndexMenuLayout) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileMenuLayout);
    } else if(event.event == BtRemotesProfileSettingsIndexPerRemote) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRemoteTypeSettings);
    } else if(event.event == BtRemotesProfileSettingsIndexProfileMgmt) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileManagement);
    } else if(event.event == BtRemotesProfileSettingsIndexHelp) {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneRemoteSettingsHelp, RemoteSettingsHelpProfile);
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRemoteSettingsHelp);
    }
    return true;
}

void bt_remotes_scene_profile_settings_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
