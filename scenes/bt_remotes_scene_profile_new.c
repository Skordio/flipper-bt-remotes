#include "../bt_remotes.h"

enum BtRemotesProfileNewEvent {
    BtRemotesProfileNewEventDone,
    BtRemotesProfileNewEventPopup,
};

static void bt_remotes_scene_profile_new_text_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtRemotesProfileNewEventDone);
}

static void bt_remotes_scene_profile_new_popup_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtRemotesProfileNewEventPopup);
}

static bool
    bt_remotes_scene_profile_new_validator(const char* text, FuriString* error, void* context) {
    Hid* app = context;

    if(!bt_remotes_validate_name(text, error)) return false;

    FuriString* path = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, text, BT_REMOTES_CFG_EXT);
    bool exists = storage_file_exists(app->storage, furi_string_get_cstr(path));
    furi_string_free(path);
    if(exists) {
        furi_string_set(error, "Name already\nin use");
        return false;
    }

    return true;
}

void bt_remotes_scene_profile_new_on_enter(void* context) {
    Hid* app = context;
    app->active_profile[0] = '\0';
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "New Profile Name");
    text_input_set_result_callback(
        app->text_input,
        bt_remotes_scene_profile_new_text_cb,
        app,
        app->active_profile,
        BT_REMOTES_PROFILE_NAME_LEN,
        true);
    text_input_set_validator(app->text_input, bt_remotes_scene_profile_new_validator, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewTextInput);
}

bool bt_remotes_scene_profile_new_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == BtRemotesProfileNewEventDone) {
            popup_reset(app->popup);
            if(bt_remotes_profile_create(app)) {
                // Load the just-created profile so every per-profile field defaults
                // from its (name+mac-only) .cfg — menu_order/menu_hidden and all
                // per-remote settings, not just BLE identity. Without this the new
                // profile would keep the previous profile's stale in-memory settings
                // (the create flow auto-advances to Start without re-activating).
                bt_remotes_profile_activate(app);
                popup_set_header(app->popup, "Profile Created!", 64, 10, AlignCenter, AlignTop);
                popup_set_text(
                    app->popup,
                    "Starting BLE...\nPair your device\nwhen ready.",
                    64,
                    28,
                    AlignCenter,
                    AlignTop);
                popup_set_timeout(app->popup, 2000);
            } else {
                // Creation failed (storage error) — show error and let popup pop the scene
                // without starting BLE or advancing to Start.
                app->active_profile[0] = '\0';
                popup_set_header(app->popup, "Error", 64, 10, AlignCenter, AlignTop);
                popup_set_text(
                    app->popup,
                    "Could not create\nprofile. Check\nstorage.",
                    64,
                    28,
                    AlignCenter,
                    AlignTop);
                popup_set_timeout(app->popup, 2500);
            }
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, bt_remotes_scene_profile_new_popup_cb);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);

        } else if(event.event == BtRemotesProfileNewEventPopup) {
            if(app->active_profile[0] != '\0') {
                // Start BLE then pop back to profile_select, which detects ble_started
                // and auto-advances to Start.
                bt_remotes_start_ble(app);
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, BtRemotesSceneProfileSelect);
            } else {
                // Creation failed — just go back to profile select without starting BLE.
                scene_manager_search_and_switch_to_previous_scene(
                    app->scene_manager, BtRemotesSceneProfileSelect);
            }
        }
    }

    return consumed;
}

void bt_remotes_scene_profile_new_on_exit(void* context) {
    Hid* app = context;
    text_input_reset(app->text_input);
    popup_reset(app->popup);
}
