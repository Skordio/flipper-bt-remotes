#include "../bt_remotes.h"

// Keynote per-profile settings, shown as a VariableItemList so the back-key choice
// is adjusted with Left/Right. Help row sits last and opens via the enter callback;
// the change callback persists immediately.

static const char* const keynote_back_key_labels[KEYNOTE_BACK_KEY_COUNT] = {
    "Delete",
    "Left Arrow",
    "Escape",
    "None",
};

#define KEYNOTE_SETTINGS_ROW_HELP 1

static void keynote_back_key_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->keynote_back_key = idx;
    variable_item_set_current_value_text(item, keynote_back_key_labels[idx]);
    bt_remotes_save_profile_menu_cfg(app);
}

static void keynote_settings_enter_cb(void* context, uint32_t index) {
    Hid* app = context;
    if(index == KEYNOTE_SETTINGS_ROW_HELP) {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneRemoteSettingsHelp, RemoteSettingsHelpKeynote);
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRemoteSettingsHelp);
    }
}

void bt_remotes_scene_keynote_settings_on_enter(void* context) {
    Hid* app = context;
    VariableItemList* vil = app->var_item_list;
    variable_item_list_reset(vil);
    variable_item_list_set_header(vil, "Keynote");

    VariableItem* item = variable_item_list_add(
        vil, "Back Key", KEYNOTE_BACK_KEY_COUNT, keynote_back_key_changed, app);
    variable_item_set_current_value_index(item, app->keynote_back_key);
    variable_item_set_current_value_text(item, keynote_back_key_labels[app->keynote_back_key]);

    // Help (no value; OK opens the help page via the enter callback).
    variable_item_list_add(vil, "Help", 1, NULL, app);

    variable_item_list_set_enter_callback(vil, keynote_settings_enter_cb, app);
    variable_item_list_set_selected_item(vil, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewVariableItemList);
}

bool bt_remotes_scene_keynote_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void bt_remotes_scene_keynote_settings_on_exit(void* context) {
    Hid* app = context;
    variable_item_list_reset(app->var_item_list);
}
