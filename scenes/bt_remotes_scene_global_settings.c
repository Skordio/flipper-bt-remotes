#include "../bt_remotes.h"

// App-wide settings, reached from Profile Select. No active profile is required
// (and is intentionally ignored). Per-profile settings live under the active
// profile's Start menu via BtRemotesSceneProfileSettings.
//
// VariableItemList scene modeled on scene_ios_phone_settings.c: Vibration and
// Hold Back Quit are Left/Right-adjustable rows; Default BT Name and Help open
// via the enter callback. Each change persists immediately through
// bt_remotes_save_app_cfg.

#define GLOBAL_ROW_DEFAULT_NAME 0
#define GLOBAL_ROW_VIBRATION    1
#define GLOBAL_ROW_HOLD_QUIT    2
#define GLOBAL_ROW_HELP         3

static const char* const bt_remotes_vibro_mode_labels[] = {
    "Neither",
    "Disconnect",
    "Connect",
    "Both",
};

static void global_vibro_mode_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->vibro_mode = idx;
    variable_item_set_current_value_text(item, bt_remotes_vibro_mode_labels[idx]);
    bt_remotes_save_app_cfg(app);
}

static void global_back_hold_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint8_t val = BT_REMOTES_BACK_HOLD_EXIT_S_MIN + idx;
    app->back_hold_exit_s = val;
    FuriString* s = furi_string_alloc_printf("%us", (unsigned)val);
    variable_item_set_current_value_text(item, furi_string_get_cstr(s));
    furi_string_free(s);
    bt_remotes_save_app_cfg(app);
}

static void global_settings_enter_cb(void* context, uint32_t index) {
    Hid* app = context;
    if(index == GLOBAL_ROW_DEFAULT_NAME) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRename);
    } else if(index == GLOBAL_ROW_HELP) {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneRemoteSettingsHelp, RemoteSettingsHelpGlobal);
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRemoteSettingsHelp);
    }
}

void bt_remotes_scene_global_settings_on_enter(void* context) {
    Hid* app = context;
    VariableItemList* vil = app->var_item_list;
    variable_item_list_reset(vil);
    variable_item_list_set_header(vil, "Global Settings");

    VariableItem* item;

    // Default BT Name (no value; OK opens the rename keyboard via the enter callback).
    variable_item_list_add(vil, "Default BT Name", 1, NULL, app);

    item = variable_item_list_add(vil, "Vibration", 4, global_vibro_mode_changed, app);
    variable_item_set_current_value_index(item, app->vibro_mode);
    variable_item_set_current_value_text(item, bt_remotes_vibro_mode_labels[app->vibro_mode]);

    item = variable_item_list_add(
        vil,
        "Hold Back Quit",
        BT_REMOTES_BACK_HOLD_EXIT_S_MAX - BT_REMOTES_BACK_HOLD_EXIT_S_MIN + 1,
        global_back_hold_changed,
        app);
    {
        uint8_t cur = app->back_hold_exit_s;
        if(cur < BT_REMOTES_BACK_HOLD_EXIT_S_MIN) cur = BT_REMOTES_BACK_HOLD_EXIT_S_MIN;
        if(cur > BT_REMOTES_BACK_HOLD_EXIT_S_MAX) cur = BT_REMOTES_BACK_HOLD_EXIT_S_MAX;
        variable_item_set_current_value_index(item, cur - BT_REMOTES_BACK_HOLD_EXIT_S_MIN);
        FuriString* s = furi_string_alloc_printf("%us", (unsigned)cur);
        variable_item_set_current_value_text(item, furi_string_get_cstr(s));
        furi_string_free(s);
    }

    // Help (no value; OK opens the help page via the enter callback).
    variable_item_list_add(vil, "Help", 1, NULL, app);

    variable_item_list_set_enter_callback(vil, global_settings_enter_cb, app);
    // Restore cursor to wherever the user left it (e.g. after coming back from Help).
    variable_item_list_set_selected_item(
        vil, scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneGlobalSettings));
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewVariableItemList);
}

bool bt_remotes_scene_global_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void bt_remotes_scene_global_settings_on_exit(void* context) {
    Hid* app = context;
    // Save cursor before reset so it survives navigation to and back from any
    // sub-scene (Help, Default BT Name, etc.).
    scene_manager_set_scene_state(
        app->scene_manager,
        BtRemotesSceneGlobalSettings,
        variable_item_list_get_selected_item_index(app->var_item_list));
    variable_item_list_reset(app->var_item_list);
}
