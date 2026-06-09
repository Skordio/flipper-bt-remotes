#include "../bt_remotes.h"

// DuckyScript per-profile settings, shown as a VariableItemList. Rows:
//   Connect Per Run - stay disconnected while browsing; connect only for a run
//   Connect Delay   - ms to wait after the link is up before sending HID, so the
//                     host finishes HID discovery first (only used in Per-Run mode)
// Changes are applied immediately to in-memory state; persisted on scene exit.

static void ducky_per_run_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->ducky_connect_per_run = idx;
    variable_item_set_current_value_text(item, idx ? "On" : "Off");
}

static void ducky_settle_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint16_t val = (uint16_t)(DUCKY_CONNECT_SETTLE_MIN + idx * DUCKY_CONNECT_SETTLE_STEP);
    app->ducky_connect_settle_ms = val;
    char buf[12];
    snprintf(buf, sizeof(buf), "%u ms", (unsigned)val);
    variable_item_set_current_value_text(item, buf);
}

// The "Help" row sits after the adjustable rows; OK on it opens the shared
// Per-Remote Settings Help page. The other rows adjust with Left/Right.
#define DUCKY_SETTINGS_ROW_HELP 2

static void ducky_settings_enter_cb(void* context, uint32_t index) {
    Hid* app = context;
    if(index == DUCKY_SETTINGS_ROW_HELP) {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneRemoteSettingsHelp, RemoteSettingsHelpDucky);
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRemoteSettingsHelp);
    }
}

void bt_remotes_scene_ducky_settings_on_enter(void* context) {
    Hid* app = context;
    VariableItemList* vil = app->var_item_list;
    variable_item_list_reset(vil);
    variable_item_list_set_header(vil, "DuckyScript");

    VariableItem* item;
    char buf[12];

    // Connect Per Run
    item = variable_item_list_add(vil, "Connect Per Run", 2, ducky_per_run_changed, app);
    variable_item_set_current_value_index(item, app->ducky_connect_per_run);
    variable_item_set_current_value_text(item, app->ducky_connect_per_run ? "On" : "Off");

    // Connect Delay (ms)
    item = variable_item_list_add(
        vil, "Connect Delay", DUCKY_CONNECT_SETTLE_COUNT, ducky_settle_changed, app);
    variable_item_set_current_value_index(
        item, (app->ducky_connect_settle_ms - DUCKY_CONNECT_SETTLE_MIN) / DUCKY_CONNECT_SETTLE_STEP);
    snprintf(buf, sizeof(buf), "%u ms", (unsigned)app->ducky_connect_settle_ms);
    variable_item_set_current_value_text(item, buf);

    // Help (no value; OK opens the help page via the enter callback)
    variable_item_list_add(vil, "Help", 1, NULL, app);

    variable_item_list_set_enter_callback(vil, ducky_settings_enter_cb, app);
    variable_item_list_set_selected_item(vil, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewVariableItemList);
}

bool bt_remotes_scene_ducky_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    // All edits happen in the VariableItem change callbacks; nothing to consume.
    return false;
}

void bt_remotes_scene_ducky_settings_on_exit(void* context) {
    Hid* app = context;
    bt_remotes_save_profile_menu_cfg(app);
    variable_item_list_reset(app->var_item_list);
}
