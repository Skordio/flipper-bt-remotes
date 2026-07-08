#include "../bt_remotes.h"

// iOS Phone per-profile settings, modeled on scene_media_settings.c. Cursor
// speed/ramp + Swipe distance + Double-Tap window are VariableItemList ranges;
// Cursor Mode and Dbl Tap Swipe are toggles. Help opens via the enter callback.
// Each change persists immediately through bt_remotes_save_profile_menu_cfg so
// the value survives profile_activate.

#define IOS_PHONE_ROW_CURSOR_MODE 0
#define IOS_PHONE_ROW_CURSOR_SPD  1
#define IOS_PHONE_ROW_RAMP_START  2
#define IOS_PHONE_ROW_RAMP_TIME   3
#define IOS_PHONE_ROW_SWIPE       4
#define IOS_PHONE_ROW_SWIPE_SPD   5
#define IOS_PHONE_ROW_DBL_TAP     6
#define IOS_PHONE_ROW_DBL_SWIPE   7
#define IOS_PHONE_ROW_HELP        8

static const char* const ios_cursor_mode_labels[IOS_CURSOR_MODE_COUNT] = {
    "Constant",
    "Ramp",
};

static void ios_cursor_mode_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->ios_cursor_mode = idx;
    variable_item_set_current_value_text(item, ios_cursor_mode_labels[idx]);
    bt_remotes_save_profile_menu_cfg(app);
}

static void ios_cursor_speed_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint16_t val = IOS_CURSOR_SPEED_MIN + (uint16_t)idx * IOS_CURSOR_SPEED_STEP;
    app->ios_cursor_speed_px_s = val;
    FuriString* s = furi_string_alloc_printf("%u", val);
    variable_item_set_current_value_text(item, furi_string_get_cstr(s));
    furi_string_free(s);
    bt_remotes_save_profile_menu_cfg(app);
}

static void ios_ramp_start_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint8_t val = IOS_RAMP_START_PCT_MIN + idx * IOS_RAMP_START_PCT_STEP;
    app->ios_ramp_start_pct = val;
    FuriString* s = furi_string_alloc_printf("%u%%", val);
    variable_item_set_current_value_text(item, furi_string_get_cstr(s));
    furi_string_free(s);
    bt_remotes_save_profile_menu_cfg(app);
}

static void ios_ramp_time_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint16_t val = IOS_RAMP_TIME_MIN + (uint16_t)idx * IOS_RAMP_TIME_STEP;
    app->ios_ramp_time_ms = val;
    FuriString* s = furi_string_alloc_printf("%u", val);
    variable_item_set_current_value_text(item, furi_string_get_cstr(s));
    furi_string_free(s);
    bt_remotes_save_profile_menu_cfg(app);
}

static void ios_swipe_distance_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint16_t val = IOS_SWIPE_DISTANCE_MIN + (uint16_t)idx * IOS_SWIPE_DISTANCE_STEP;
    app->ios_swipe_distance = val;
    FuriString* s = furi_string_alloc_printf("%u", val);
    variable_item_set_current_value_text(item, furi_string_get_cstr(s));
    furi_string_free(s);
    bt_remotes_save_profile_menu_cfg(app);
}

static void ios_swipe_speed_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint16_t val = IOS_SWIPE_SPEED_MIN + (uint16_t)idx * IOS_SWIPE_SPEED_STEP;
    app->ios_swipe_speed_px_s = val;
    FuriString* s = furi_string_alloc_printf("%u", val);
    variable_item_set_current_value_text(item, furi_string_get_cstr(s));
    furi_string_free(s);
    bt_remotes_save_profile_menu_cfg(app);
}

static void ios_dbl_tap_window_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint16_t val = IOS_DBL_TAP_WINDOW_MIN + (uint16_t)idx * IOS_DBL_TAP_WINDOW_STEP;
    app->ios_dbl_tap_window_ms = val;
    FuriString* s = furi_string_alloc_printf("%u", val);
    variable_item_set_current_value_text(item, furi_string_get_cstr(s));
    furi_string_free(s);
    bt_remotes_save_profile_menu_cfg(app);
}

static void ios_dbl_tap_swipe_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->ios_dbl_tap_swipe = idx;
    variable_item_set_current_value_text(item, idx ? "On" : "Off");
    bt_remotes_save_profile_menu_cfg(app);
}

static void ios_phone_settings_enter_cb(void* context, uint32_t index) {
    Hid* app = context;
    if(index == IOS_PHONE_ROW_HELP) {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneRemoteSettingsHelp, RemoteSettingsHelpIosPhone);
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRemoteSettingsHelp);
    }
}

void bt_remotes_scene_ios_phone_settings_on_enter(void* context) {
    Hid* app = context;
    VariableItemList* vil = app->var_item_list;
    variable_item_list_reset(vil);
    variable_item_list_set_header(vil, "iOS Phone");

    VariableItem* item;
    FuriString*   s;

    item = variable_item_list_add(
        vil, "Cursor Mode", IOS_CURSOR_MODE_COUNT, ios_cursor_mode_changed, app);
    {
        uint8_t cur = app->ios_cursor_mode;
        if(cur >= IOS_CURSOR_MODE_COUNT) cur = IOS_CURSOR_MODE_DEFAULT;
        variable_item_set_current_value_index(item, cur);
        variable_item_set_current_value_text(item, ios_cursor_mode_labels[cur]);
    }

    item = variable_item_list_add(
        vil,
        "Cursor Spd",
        IOS_VALUE_COUNT(IOS_CURSOR_SPEED_MIN, IOS_CURSOR_SPEED_MAX, IOS_CURSOR_SPEED_STEP),
        ios_cursor_speed_changed,
        app);
    {
        uint16_t cur = app->ios_cursor_speed_px_s;
        if(cur < IOS_CURSOR_SPEED_MIN) cur = IOS_CURSOR_SPEED_MIN;
        if(cur > IOS_CURSOR_SPEED_MAX) cur = IOS_CURSOR_SPEED_MAX;
        uint8_t idx = (uint8_t)((cur - IOS_CURSOR_SPEED_MIN) / IOS_CURSOR_SPEED_STEP);
        variable_item_set_current_value_index(item, idx);
        s = furi_string_alloc_printf("%u", cur);
        variable_item_set_current_value_text(item, furi_string_get_cstr(s));
        furi_string_free(s);
    }

    item = variable_item_list_add(
        vil,
        "Ramp Start",
        IOS_VALUE_COUNT(IOS_RAMP_START_PCT_MIN, IOS_RAMP_START_PCT_MAX, IOS_RAMP_START_PCT_STEP),
        ios_ramp_start_changed,
        app);
    {
        uint8_t cur = app->ios_ramp_start_pct;
        if(cur < IOS_RAMP_START_PCT_MIN) cur = IOS_RAMP_START_PCT_MIN;
        if(cur > IOS_RAMP_START_PCT_MAX) cur = IOS_RAMP_START_PCT_MAX;
        uint8_t idx = (uint8_t)((cur - IOS_RAMP_START_PCT_MIN) / IOS_RAMP_START_PCT_STEP);
        variable_item_set_current_value_index(item, idx);
        s = furi_string_alloc_printf("%u%%", cur);
        variable_item_set_current_value_text(item, furi_string_get_cstr(s));
        furi_string_free(s);
    }

    item = variable_item_list_add(
        vil,
        "Ramp Time",
        IOS_VALUE_COUNT(IOS_RAMP_TIME_MIN, IOS_RAMP_TIME_MAX, IOS_RAMP_TIME_STEP),
        ios_ramp_time_changed,
        app);
    {
        uint16_t cur = app->ios_ramp_time_ms;
        if(cur < IOS_RAMP_TIME_MIN) cur = IOS_RAMP_TIME_MIN;
        if(cur > IOS_RAMP_TIME_MAX) cur = IOS_RAMP_TIME_MAX;
        uint8_t idx = (uint8_t)((cur - IOS_RAMP_TIME_MIN) / IOS_RAMP_TIME_STEP);
        variable_item_set_current_value_index(item, idx);
        s = furi_string_alloc_printf("%u", cur);
        variable_item_set_current_value_text(item, furi_string_get_cstr(s));
        furi_string_free(s);
    }

    item = variable_item_list_add(
        vil,
        "Swipe Dist",
        IOS_VALUE_COUNT(IOS_SWIPE_DISTANCE_MIN, IOS_SWIPE_DISTANCE_MAX, IOS_SWIPE_DISTANCE_STEP),
        ios_swipe_distance_changed,
        app);
    {
        uint16_t cur = app->ios_swipe_distance;
        if(cur < IOS_SWIPE_DISTANCE_MIN) cur = IOS_SWIPE_DISTANCE_MIN;
        if(cur > IOS_SWIPE_DISTANCE_MAX) cur = IOS_SWIPE_DISTANCE_MAX;
        uint8_t idx = (uint8_t)((cur - IOS_SWIPE_DISTANCE_MIN) / IOS_SWIPE_DISTANCE_STEP);
        variable_item_set_current_value_index(item, idx);
        s = furi_string_alloc_printf("%u", cur);
        variable_item_set_current_value_text(item, furi_string_get_cstr(s));
        furi_string_free(s);
    }

    item = variable_item_list_add(
        vil,
        "Swipe Spd",
        IOS_VALUE_COUNT(IOS_SWIPE_SPEED_MIN, IOS_SWIPE_SPEED_MAX, IOS_SWIPE_SPEED_STEP),
        ios_swipe_speed_changed,
        app);
    {
        uint16_t cur = app->ios_swipe_speed_px_s;
        if(cur < IOS_SWIPE_SPEED_MIN) cur = IOS_SWIPE_SPEED_MIN;
        if(cur > IOS_SWIPE_SPEED_MAX) cur = IOS_SWIPE_SPEED_MAX;
        uint8_t idx = (uint8_t)((cur - IOS_SWIPE_SPEED_MIN) / IOS_SWIPE_SPEED_STEP);
        variable_item_set_current_value_index(item, idx);
        s = furi_string_alloc_printf("%u", cur);
        variable_item_set_current_value_text(item, furi_string_get_cstr(s));
        furi_string_free(s);
    }

    item = variable_item_list_add(
        vil,
        "Dbl Tap ms",
        IOS_VALUE_COUNT(IOS_DBL_TAP_WINDOW_MIN, IOS_DBL_TAP_WINDOW_MAX, IOS_DBL_TAP_WINDOW_STEP),
        ios_dbl_tap_window_changed,
        app);
    {
        uint16_t cur = app->ios_dbl_tap_window_ms;
        if(cur < IOS_DBL_TAP_WINDOW_MIN) cur = IOS_DBL_TAP_WINDOW_MIN;
        if(cur > IOS_DBL_TAP_WINDOW_MAX) cur = IOS_DBL_TAP_WINDOW_MAX;
        uint8_t idx = (uint8_t)((cur - IOS_DBL_TAP_WINDOW_MIN) / IOS_DBL_TAP_WINDOW_STEP);
        variable_item_set_current_value_index(item, idx);
        s = furi_string_alloc_printf("%u", cur);
        variable_item_set_current_value_text(item, furi_string_get_cstr(s));
        furi_string_free(s);
    }

    item = variable_item_list_add(vil, "Dbl Tap Swipe", 2, ios_dbl_tap_swipe_changed, app);
    variable_item_set_current_value_index(item, app->ios_dbl_tap_swipe ? 1 : 0);
    variable_item_set_current_value_text(item, app->ios_dbl_tap_swipe ? "On" : "Off");

    // Help (no value; OK opens the help page via the enter callback).
    variable_item_list_add(vil, "Help", 1, NULL, app);

    variable_item_list_set_enter_callback(vil, ios_phone_settings_enter_cb, app);
    variable_item_list_set_selected_item(
        vil,
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneIosPhoneSettings));
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewVariableItemList);
}

bool bt_remotes_scene_ios_phone_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void bt_remotes_scene_ios_phone_settings_on_exit(void* context) {
    Hid* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        BtRemotesSceneIosPhoneSettings,
        variable_item_list_get_selected_item_index(app->var_item_list));
    variable_item_list_reset(app->var_item_list);
}
