#include "../bt_remotes.h"

static const char* const bt_remotes_vibro_mode_labels[] = {
    "Vibration: Neither",
    "Vibration: Disconnect",
    "Vibration: Connect",
    "Vibration: Both",
};

enum BtRemotesSettingsIndex {
    BtRemotesSettingsIndexBluetoothName,
    BtRemotesSettingsIndexDisconnectVibro,
    BtRemotesSettingsIndexHideItems,
    BtRemotesSettingsIndexRemoteTypeSettings,
    BtRemotesSettingsIndexDelayConnect,
    BtRemotesSettingsIndexResetMenu,
    BtRemotesSettingsIndexRenameProfile,
    BtRemotesSettingsIndexUnpair,
    BtRemotesSettingsIndexSaveProfile,
    BtRemotesSettingsIndexDeleteProfile,
};

// Dynamic label for the Delay Connect toggle. submenu_add_item stores the pointer
// (it does not copy), so the buffer must outlive the submenu — keep it file-scope.
static char bt_remotes_delay_connect_label[40];

static void bt_remotes_scene_settings_submenu_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void build_settings_menu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Settings");

    submenu_add_item(
        app->submenu,
        "Bluetooth Name",
        BtRemotesSettingsIndexBluetoothName,
        bt_remotes_scene_settings_submenu_cb,
        app);

    submenu_add_item(
        app->submenu,
        bt_remotes_vibro_mode_labels[app->vibro_mode],
        BtRemotesSettingsIndexDisconnectVibro,
        bt_remotes_scene_settings_submenu_cb,
        app);

    if(app->active_profile[0] != '\0') {
        submenu_add_item(
            app->submenu,
            "Hide Remote Types",
            BtRemotesSettingsIndexHideItems,
            bt_remotes_scene_settings_submenu_cb,
            app);
        submenu_add_item(
            app->submenu,
            "Per-Remote Settings",
            BtRemotesSettingsIndexRemoteTypeSettings,
            bt_remotes_scene_settings_submenu_cb,
            app);
        snprintf(
            bt_remotes_delay_connect_label,
            sizeof(bt_remotes_delay_connect_label),
            "Delay Connect: %s",
            app->delay_connect ? "On" : "Off");
        submenu_add_item(
            app->submenu,
            bt_remotes_delay_connect_label,
            BtRemotesSettingsIndexDelayConnect,
            bt_remotes_scene_settings_submenu_cb,
            app);
        submenu_add_item(
            app->submenu,
            "Reset Menu Order",
            BtRemotesSettingsIndexResetMenu,
            bt_remotes_scene_settings_submenu_cb,
            app);
        submenu_add_item(
            app->submenu,
            "Rename Profile",
            BtRemotesSettingsIndexRenameProfile,
            bt_remotes_scene_settings_submenu_cb,
            app);
        submenu_add_item(
            app->submenu,
            "Bluetooth Unpairing",
            BtRemotesSettingsIndexUnpair,
            bt_remotes_scene_settings_submenu_cb,
            app);
        submenu_add_item(
            app->submenu,
            "Save Profile",
            BtRemotesSettingsIndexSaveProfile,
            bt_remotes_scene_settings_submenu_cb,
            app);
        submenu_add_item(
            app->submenu,
            "Delete Profile",
            BtRemotesSettingsIndexDeleteProfile,
            bt_remotes_scene_settings_submenu_cb,
            app);
    }
}

void bt_remotes_scene_settings_on_enter(void* context) {
    Hid* app = context;

    build_settings_menu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_settings_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Restore the session's BLE identity on the way out of Settings. Delay-connect
        // profiles stay disconnected at the menu, so don't auto-start there (the Start
        // scene's on_enter keeps BLE stopped regardless; skipping avoids a start/stop blip).
        if(!app->ble_started && app->active_profile[0] != '\0' && !app->delay_connect) {
            bt_remotes_profile_activate(app);
            bt_remotes_start_ble(app);
        }
        return false;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;

        if(event.event == BtRemotesSettingsIndexBluetoothName) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneRename);
        } else if(event.event == BtRemotesSettingsIndexDisconnectVibro) {
            app->vibro_mode = (app->vibro_mode + 1) % 4;
            bt_remotes_save_app_cfg(app);
            build_settings_menu(app);
            submenu_set_selected_item(app->submenu, BtRemotesSettingsIndexDisconnectVibro);
        } else if(event.event == BtRemotesSettingsIndexHideItems) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneHideItems);
        } else if(event.event == BtRemotesSettingsIndexRemoteTypeSettings) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneRemoteTypeSettings);
        } else if(event.event == BtRemotesSettingsIndexDelayConnect) {
            app->delay_connect = app->delay_connect ? 0 : 1;
            bt_remotes_save_profile_menu_cfg(app);
            build_settings_menu(app);
            submenu_set_selected_item(app->submenu, BtRemotesSettingsIndexDelayConnect);
        } else if(event.event == BtRemotesSettingsIndexResetMenu) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneResetMenu);
        } else if(event.event == BtRemotesSettingsIndexRenameProfile) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileRenameFile);
        } else if(event.event == BtRemotesSettingsIndexUnpair) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneUnpair);
        } else if(event.event == BtRemotesSettingsIndexSaveProfile) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneSaveProfile);
        } else if(event.event == BtRemotesSettingsIndexDeleteProfile) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneDeleteProfile);
        }
    }

    return consumed;
}

void bt_remotes_scene_settings_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
