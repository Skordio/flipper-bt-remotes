#include "../bt_remotes.h"
#include "../views/hid_remote_menu.h"

// Sentinel index values for the fixed items at the bottom of the menu.
// Must not collide with valid profile list indices (0..BT_REMOTES_PROFILE_MAX_COUNT-1 = 0..15).
#define PROFILE_SELECT_IDX_HELP     0xFC // "? Help"
#define PROFILE_SELECT_IDX_NEW      0xFD // "+ New Profile"
#define PROFILE_SELECT_IDX_SETTINGS 0xFE // "Settings"

enum BtRemotesProfileSelectEvent {
    BtRemotesProfileSelectEventChosen,
    BtRemotesProfileSelectEventNew,
    BtRemotesProfileSelectEventAutoAdvance,
    BtRemotesProfileSelectEventSettings,
    BtRemotesProfileSelectEventHelp,
};

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

static void profile_select_cb(void* context, uint8_t index_value) {
    Hid* app = context;
    if(index_value == PROFILE_SELECT_IDX_NEW) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BtRemotesProfileSelectEventNew);
    } else if(index_value == PROFILE_SELECT_IDX_SETTINGS) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BtRemotesProfileSelectEventSettings);
    } else if(index_value == PROFILE_SELECT_IDX_HELP) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BtRemotesProfileSelectEventHelp);
    } else {
        // index_value is the profile list position
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneProfileSelect, index_value);
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BtRemotesProfileSelectEventChosen);
    }
}

static void
    profile_reorder_cb(void* context, const uint8_t* new_order, uint8_t count) {
    Hid* app = context;

    // new_order[i] = original profile_list index of the item now at position i.
    // profile_list[] hasn't been touched yet — rebuild it in the new order.
    char tmp[BT_REMOTES_PROFILE_MAX_COUNT][BT_REMOTES_PROFILE_NAME_LEN];
    uint8_t n = (count < BT_REMOTES_PROFILE_MAX_COUNT) ? count : BT_REMOTES_PROFILE_MAX_COUNT;
    for(uint8_t i = 0; i < n; i++) {
        uint8_t src = new_order[i];
        if(src < app->profile_count) {
            strlcpy(tmp[i], app->profile_list[src], BT_REMOTES_PROFILE_NAME_LEN);
        } else {
            tmp[i][0] = '\0';
        }
    }
    for(uint8_t i = 0; i < n; i++) {
        strlcpy(app->profile_list[i], tmp[i], BT_REMOTES_PROFILE_NAME_LEN);
    }
    app->profile_count = n;

    // Rebuild profile_order_str from the new profile_list order
    app->profile_order_str[0] = '\0';
    for(uint8_t i = 0; i < app->profile_count; i++) {
        if(i > 0) strlcat(app->profile_order_str, "|", sizeof(app->profile_order_str));
        strlcat(app->profile_order_str, app->profile_list[i], sizeof(app->profile_order_str));
    }

    bt_remotes_save_app_cfg(app);
}

// ---------------------------------------------------------------------------
// Build item table for the custom menu view
// ---------------------------------------------------------------------------

static void setup_profile_menu(Hid* app) {
    // Profiles are reorderable; "Help", "New Profile", and "Settings" are fixed at the bottom.
    uint8_t total = app->profile_count + 3; // profiles + Help + New Profile + Settings

    // Stack-allocate the entries and order arrays
    BtRemotesMenuEntry entries[BT_REMOTES_PROFILE_MAX_COUNT + 3];
    uint8_t order[BT_REMOTES_PROFILE_MAX_COUNT + 3];

    for(uint8_t i = 0; i < app->profile_count; i++) {
        entries[i].label = app->profile_list[i];
        entries[i].index = i;
        order[i] = i;
    }

    uint8_t base = app->profile_count;

    entries[base].label = "? Help";
    entries[base].index = PROFILE_SELECT_IDX_HELP;
    order[base] = PROFILE_SELECT_IDX_HELP;

    entries[base + 1].label = "+ New Profile";
    entries[base + 1].index = PROFILE_SELECT_IDX_NEW;
    order[base + 1] = PROFILE_SELECT_IDX_NEW;

    entries[base + 2].label = "Settings";
    entries[base + 2].index = PROFILE_SELECT_IDX_SETTINGS;
    order[base + 2] = PROFILE_SELECT_IDX_SETTINGS;

    hid_remote_menu_set_items(app->hid_remote_menu, entries, order, total, 3);
    hid_remote_menu_set_select_callback(app->hid_remote_menu, profile_select_cb, app);
    hid_remote_menu_set_reorder_callback(app->hid_remote_menu, profile_reorder_cb, app);

    // Restore cursor to the active profile (if any)
    uint8_t cursor_pos = 0;
    if(app->active_profile[0] != '\0') {
        for(uint8_t i = 0; i < app->profile_count; i++) {
            if(strcmp(app->profile_list[i], app->active_profile) == 0) {
                cursor_pos = i;
                break;
            }
        }
    }
    hid_remote_menu_set_selected_index(app->hid_remote_menu, cursor_pos);
}

// ---------------------------------------------------------------------------
// Scene handlers
// ---------------------------------------------------------------------------

void bt_remotes_scene_profile_select_on_enter(void* context) {
    Hid* app = context;

    // BLE already running means we arrived here after a profile was just created
    // (profile_new popped back). Skip straight to Start.
    if(app->ble_started) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BtRemotesProfileSelectEventAutoAdvance);
        return;
    }

    bt_remotes_profile_load_list(app); // also applies saved profile_order_str
    setup_profile_menu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewRemoteMenu);
}

bool bt_remotes_scene_profile_select_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;

        if(event.event == BtRemotesProfileSelectEventAutoAdvance) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneStart);

        } else if(event.event == BtRemotesProfileSelectEventChosen) {
            uint32_t idx = scene_manager_get_scene_state(
                app->scene_manager, BtRemotesSceneProfileSelect);
            strlcpy(
                app->active_profile, app->profile_list[idx], BT_REMOTES_PROFILE_NAME_LEN);

            if(!bt_remotes_profile_activate(app)) {
                FURI_LOG_E("BtRemotes", "Failed to activate profile: %s", app->active_profile);
                app->active_profile[0] = '\0';
                // Stay on profile select — view is still visible
            } else {
                // Load per-profile active custom remotes before entering Start
                bt_remotes_active_remotes_load(app);
                bt_remotes_start_ble(app);
                scene_manager_next_scene(app->scene_manager, BtRemotesSceneStart);
            }

        } else if(event.event == BtRemotesProfileSelectEventNew) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileNew);

        } else if(event.event == BtRemotesProfileSelectEventSettings) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneSettings);

        } else if(event.event == BtRemotesProfileSelectEventHelp) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneHelp);
        }
    }

    return consumed;
}

void bt_remotes_scene_profile_select_on_exit(void* context) {
    UNUSED(context);
    // HidRemoteMenu has no reset needed
}
