#include "../bt_remotes.h"

// Connection sub-scene under Profile Settings. Bluetooth name (per-profile, not
// the global default), Delay Connect toggle, and Bluetooth Unpairing.

// submenu_add_item stores the pointer, not a copy — keep the toggle label buffer
// file-scope so it outlives the submenu.
static char bt_remotes_delay_connect_label[40];

enum BtRemotesProfileConnectionIndex {
    BtRemotesProfileConnectionIndexBluetoothName,
    BtRemotesProfileConnectionIndexDelayConnect,
    BtRemotesProfileConnectionIndexSaveBtKeys,
    BtRemotesProfileConnectionIndexUnpair,
};

static void bt_remotes_scene_profile_connection_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void build_connection_menu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Connection");

    submenu_add_item(
        app->submenu,
        "Bluetooth Name",
        BtRemotesProfileConnectionIndexBluetoothName,
        bt_remotes_scene_profile_connection_cb,
        app);

    snprintf(
        bt_remotes_delay_connect_label,
        sizeof(bt_remotes_delay_connect_label),
        "Delay Connect: %s",
        app->delay_connect ? "On" : "Off");
    submenu_add_item(
        app->submenu,
        bt_remotes_delay_connect_label,
        BtRemotesProfileConnectionIndexDelayConnect,
        bt_remotes_scene_profile_connection_cb,
        app);

    submenu_add_item(
        app->submenu,
        "Save BT Keys",
        BtRemotesProfileConnectionIndexSaveBtKeys,
        bt_remotes_scene_profile_connection_cb,
        app);

    submenu_add_item(
        app->submenu,
        "Bluetooth Unpairing",
        BtRemotesProfileConnectionIndexUnpair,
        bt_remotes_scene_profile_connection_cb,
        app);
}

void bt_remotes_scene_profile_connection_on_enter(void* context) {
    Hid* app = context;
    build_connection_menu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_profile_connection_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == BtRemotesProfileConnectionIndexBluetoothName) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRename);
    } else if(event.event == BtRemotesProfileConnectionIndexDelayConnect) {
        app->delay_connect = app->delay_connect ? 0 : 1;
        bt_remotes_save_profile_menu_cfg(app);
        build_connection_menu(app);
        submenu_set_selected_item(app->submenu, BtRemotesProfileConnectionIndexDelayConnect);
    } else if(event.event == BtRemotesProfileConnectionIndexSaveBtKeys) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneSaveProfile);
    } else if(event.event == BtRemotesProfileConnectionIndexUnpair) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneUnpair);
    }
    return true;
}

void bt_remotes_scene_profile_connection_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
