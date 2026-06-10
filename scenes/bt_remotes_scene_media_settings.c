#include "../bt_remotes.h"

// Media per-profile settings, shown as a VariableItemList so each value is
// adjusted with Left/Right. Help row sits last and opens via the enter callback;
// each change callback persists immediately.

#define MEDIA_SETTINGS_ROW_HELP 2

static void media_mode_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->media_mode = idx;
    variable_item_set_current_value_text(item, (idx == MediaModeImproved) ? "Improved" : "Legacy");
    bt_remotes_save_profile_menu_cfg(app);
}

static void media_mouse_switch_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->media_mouse_switch = idx;
    variable_item_set_current_value_text(item, idx ? "On" : "Off");
    bt_remotes_save_profile_menu_cfg(app);
}

static void media_settings_enter_cb(void* context, uint32_t index) {
    Hid* app = context;
    if(index == MEDIA_SETTINGS_ROW_HELP) {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneRemoteSettingsHelp, RemoteSettingsHelpMedia);
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRemoteSettingsHelp);
    }
}

void bt_remotes_scene_media_settings_on_enter(void* context) {
    Hid* app = context;
    VariableItemList* vil = app->var_item_list;
    variable_item_list_reset(vil);
    variable_item_list_set_header(vil, "Media");

    VariableItem* item;

    item = variable_item_list_add(vil, "Mode", MEDIA_MODE_COUNT, media_mode_changed, app);
    variable_item_set_current_value_index(item, app->media_mode);
    variable_item_set_current_value_text(
        item, (app->media_mode == MediaModeImproved) ? "Improved" : "Legacy");

    item = variable_item_list_add(vil, "Mouse Switcher", 2, media_mouse_switch_changed, app);
    variable_item_set_current_value_index(item, app->media_mouse_switch);
    variable_item_set_current_value_text(item, app->media_mouse_switch ? "On" : "Off");

    // Help (no value; OK opens the help page via the enter callback).
    variable_item_list_add(vil, "Help", 1, NULL, app);

    variable_item_list_set_enter_callback(vil, media_settings_enter_cb, app);
    variable_item_list_set_selected_item(vil, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewVariableItemList);
}

bool bt_remotes_scene_media_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void bt_remotes_scene_media_settings_on_exit(void* context) {
    Hid* app = context;
    variable_item_list_reset(app->var_item_list);
}
