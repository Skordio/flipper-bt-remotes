#include "../bt_remotes.h"

// Replaces the active profile's Bluetooth Name with the global Default Bluetooth
// Name and re-advertises. Mirrors the structure of scene_save_profile: do the
// work in on_enter, then show a "Done!" popup that pops back on timeout.

enum BtRemotesResetBtNameEvent {
    BtRemotesResetBtNameEventDone,
};

static void bt_remotes_scene_reset_bt_name_popup_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtRemotesResetBtNameEventDone);
}

void bt_remotes_scene_reset_bt_name_on_enter(void* context) {
    Hid* app = context;

    strlcpy(
        app->ble_hid_cfg.name, app->default_ble_name, sizeof(app->ble_hid_cfg.name));
    bt_hid_save_cfg(app);
    bt_remotes_save_profile_menu_cfg(app);

    // Cycle BLE so the new name advertises right away. Same pattern as
    // scene_rename.c. start_ble_if_immediate respects delay_connect.
    if(app->ble_started) bt_remotes_stop_ble(app);
    bt_remotes_profile_activate(app);
    bt_remotes_start_ble_if_immediate(app);

    popup_reset(app->popup);
    popup_set_header(app->popup, "Reset!", 64, 10, AlignCenter, AlignTop);
    popup_set_text(app->popup, app->default_ble_name, 64, 28, AlignCenter, AlignTop);
    popup_set_timeout(app->popup, 1500);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, bt_remotes_scene_reset_bt_name_popup_cb);
    popup_enable_timeout(app->popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);
}

bool bt_remotes_scene_reset_bt_name_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtRemotesResetBtNameEventDone) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void bt_remotes_scene_reset_bt_name_on_exit(void* context) {
    Hid* app = context;
    popup_reset(app->popup);
}
