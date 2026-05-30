#include "../bt_remotes.h"

// TikTok / YT Shorts per-profile settings, shown as a VariableItemList so each
// value is adjusted with Left/Right. Rows:
//   Scroll Mode   - Wheel / Gesture
//   Inward Margin - horizontal inset before the vertical swipe (gesture mode)
//   Edge Margin   - vertical travel off the edge before the button is held
//   Swipe Length  - drag distance while the button is held
// Every change is persisted immediately via bt_remotes_save_profile_menu_cfg.

static void tiktok_scroll_mode_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->tiktok_scroll_mode = idx;
    variable_item_set_current_value_text(item, (idx == TikTokScrollGesture) ? "Gesture" : "Wheel");
    bt_remotes_save_profile_menu_cfg(app);
}

static void tiktok_inset_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint16_t val = (uint16_t)(TIKTOK_GESTURE_INSET_MIN + idx * TIKTOK_GESTURE_INSET_STEP);
    app->tiktok_gesture_inset = val;
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", (unsigned)val);
    variable_item_set_current_value_text(item, buf);
    bt_remotes_save_profile_menu_cfg(app);
}

static void tiktok_margin_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint16_t val = (uint16_t)(TIKTOK_GESTURE_MARGIN_MIN + idx * TIKTOK_GESTURE_MARGIN_STEP);
    app->tiktok_gesture_margin = val;
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", (unsigned)val);
    variable_item_set_current_value_text(item, buf);
    bt_remotes_save_profile_menu_cfg(app);
}

static void tiktok_swipe_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    uint16_t val = (uint16_t)(TIKTOK_GESTURE_SWIPE_MIN + idx * TIKTOK_GESTURE_SWIPE_STEP);
    app->tiktok_gesture_swipe = val;
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", (unsigned)val);
    variable_item_set_current_value_text(item, buf);
    bt_remotes_save_profile_menu_cfg(app);
}

// The "Help" row sits after the four adjustable rows; pressing OK on it opens
// the shared Per-Remote Settings Help page. The numeric rows ignore OK (they
// adjust with Left/Right via their change callbacks).
#define TIKTOK_SETTINGS_ROW_HELP 4

static void tiktok_settings_enter_cb(void* context, uint32_t index) {
    Hid* app = context;
    if(index == TIKTOK_SETTINGS_ROW_HELP) {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneRemoteSettingsHelp, RemoteSettingsHelpTikTok);
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRemoteSettingsHelp);
    }
}

void bt_remotes_scene_tiktok_settings_on_enter(void* context) {
    Hid* app = context;
    VariableItemList* vil = app->var_item_list;
    variable_item_list_reset(vil);
    variable_item_list_set_header(vil, "TikTok Remote");

    VariableItem* item;
    char buf[8];

    // Scroll Mode
    item = variable_item_list_add(
        vil, "Scroll Mode", TIKTOK_SCROLL_MODE_COUNT, tiktok_scroll_mode_changed, app);
    variable_item_set_current_value_index(item, app->tiktok_scroll_mode);
    variable_item_set_current_value_text(
        item, (app->tiktok_scroll_mode == TikTokScrollGesture) ? "Gesture" : "Wheel");

    // Inward Margin
    item = variable_item_list_add(
        vil,
        "Inward Margin",
        TIKTOK_GESTURE_VALUE_COUNT(
            TIKTOK_GESTURE_INSET_MIN, TIKTOK_GESTURE_INSET_MAX, TIKTOK_GESTURE_INSET_STEP),
        tiktok_inset_changed,
        app);
    variable_item_set_current_value_index(
        item, (app->tiktok_gesture_inset - TIKTOK_GESTURE_INSET_MIN) / TIKTOK_GESTURE_INSET_STEP);
    snprintf(buf, sizeof(buf), "%u", (unsigned)app->tiktok_gesture_inset);
    variable_item_set_current_value_text(item, buf);

    // Edge Margin
    item = variable_item_list_add(
        vil,
        "Edge Margin",
        TIKTOK_GESTURE_VALUE_COUNT(
            TIKTOK_GESTURE_MARGIN_MIN, TIKTOK_GESTURE_MARGIN_MAX, TIKTOK_GESTURE_MARGIN_STEP),
        tiktok_margin_changed,
        app);
    variable_item_set_current_value_index(
        item,
        (app->tiktok_gesture_margin - TIKTOK_GESTURE_MARGIN_MIN) / TIKTOK_GESTURE_MARGIN_STEP);
    snprintf(buf, sizeof(buf), "%u", (unsigned)app->tiktok_gesture_margin);
    variable_item_set_current_value_text(item, buf);

    // Swipe Length
    item = variable_item_list_add(
        vil,
        "Swipe Length",
        TIKTOK_GESTURE_VALUE_COUNT(
            TIKTOK_GESTURE_SWIPE_MIN, TIKTOK_GESTURE_SWIPE_MAX, TIKTOK_GESTURE_SWIPE_STEP),
        tiktok_swipe_changed,
        app);
    variable_item_set_current_value_index(
        item, (app->tiktok_gesture_swipe - TIKTOK_GESTURE_SWIPE_MIN) / TIKTOK_GESTURE_SWIPE_STEP);
    snprintf(buf, sizeof(buf), "%u", (unsigned)app->tiktok_gesture_swipe);
    variable_item_set_current_value_text(item, buf);

    // Help (no value; OK opens the help page via the enter callback)
    variable_item_list_add(vil, "Help", 1, NULL, app);

    variable_item_list_set_enter_callback(vil, tiktok_settings_enter_cb, app);
    variable_item_list_set_selected_item(vil, 0);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewVariableItemList);
}

bool bt_remotes_scene_tiktok_settings_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    // All edits happen in the VariableItem change callbacks; nothing to consume.
    return false;
}

void bt_remotes_scene_tiktok_settings_on_exit(void* context) {
    Hid* app = context;
    variable_item_list_reset(app->var_item_list);
}
