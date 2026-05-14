#include "../bt_remotes.h"

enum BtRemotesSettingsIndex {
    BtRemotesSettingsIndexBluetoothName,
    BtRemotesSettingsIndexRenameProfile,
    BtRemotesSettingsIndexResetProfile,
};

static void bt_remotes_scene_settings_submenu_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bt_remotes_scene_settings_on_enter(void* context) {
    Hid* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Settings");
    submenu_add_item(
        app->submenu,
        "Bluetooth Name",
        BtRemotesSettingsIndexBluetoothName,
        bt_remotes_scene_settings_submenu_cb,
        app);
    if(app->active_profile[0] != '\0') {
        submenu_add_item(
            app->submenu,
            "Rename Profile",
            BtRemotesSettingsIndexRenameProfile,
            bt_remotes_scene_settings_submenu_cb,
            app);
        submenu_add_item(
            app->submenu,
            "Reset Profile",
            BtRemotesSettingsIndexResetProfile,
            bt_remotes_scene_settings_submenu_cb,
            app);
    }

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneSettings));
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_settings_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        if(!app->ble_started && app->active_profile[0] != '\0') {
            bt_remotes_profile_activate(app);
            bt_remotes_start_ble(app);
        }
        return false;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneSettings, event.event);
        consumed = true;

        if(event.event == BtRemotesSettingsIndexBluetoothName) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneRename);
        } else if(event.event == BtRemotesSettingsIndexRenameProfile) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileRenameFile);
        } else if(event.event == BtRemotesSettingsIndexResetProfile) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneResetProfile);
        }
    }

    return consumed;
}

void bt_remotes_scene_settings_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
