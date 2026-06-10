#include "../bt_remotes.h"

// App-wide settings, reached from Profile Select. No active profile is required
// (and is intentionally ignored). Per-profile settings live under the active
// profile's Start menu via BtRemotesSceneProfileSettings.

static const char* const bt_remotes_vibro_mode_labels[] = {
    "Vibration: Neither",
    "Vibration: Disconnect",
    "Vibration: Connect",
    "Vibration: Both",
};

enum BtRemotesGlobalSettingsIndex {
    BtRemotesGlobalSettingsIndexDefaultName,
    BtRemotesGlobalSettingsIndexVibration,
    BtRemotesGlobalSettingsIndexHelp,
};

static void bt_remotes_scene_global_settings_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void build_global_settings_menu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Global Settings");

    submenu_add_item(
        app->submenu,
        "Default Bluetooth Name",
        BtRemotesGlobalSettingsIndexDefaultName,
        bt_remotes_scene_global_settings_cb,
        app);

    submenu_add_item(
        app->submenu,
        bt_remotes_vibro_mode_labels[app->vibro_mode],
        BtRemotesGlobalSettingsIndexVibration,
        bt_remotes_scene_global_settings_cb,
        app);

    submenu_add_item(
        app->submenu,
        "Help",
        BtRemotesGlobalSettingsIndexHelp,
        bt_remotes_scene_global_settings_cb,
        app);
}

void bt_remotes_scene_global_settings_on_enter(void* context) {
    Hid* app = context;
    build_global_settings_menu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_global_settings_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == BtRemotesGlobalSettingsIndexDefaultName) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRename);
    } else if(event.event == BtRemotesGlobalSettingsIndexVibration) {
        app->vibro_mode = (app->vibro_mode + 1) % 4;
        bt_remotes_save_app_cfg(app);
        build_global_settings_menu(app);
        submenu_set_selected_item(app->submenu, BtRemotesGlobalSettingsIndexVibration);
    } else if(event.event == BtRemotesGlobalSettingsIndexHelp) {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneRemoteSettingsHelp, RemoteSettingsHelpGlobal);
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRemoteSettingsHelp);
    }
    return true;
}

void bt_remotes_scene_global_settings_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
