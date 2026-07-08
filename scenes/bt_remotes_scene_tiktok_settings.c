#include "../bt_remotes.h"

// TikTok / YT Shorts per-profile settings, shown as a VariableItemList so each
// value is adjusted with Left/Right. Rows:
//   Scroll Mode   - Wheel / Gesture
//   Inward Margin - horizontal inset before the vertical swipe (gesture mode only)
//   Edge Margin   - vertical travel off the edge before the button is held
//   Swipe Length  - drag distance while the button is held
// Every change is persisted immediately via bt_remotes_save_profile_menu_cfg.
//
// Inward Margin / Edge Margin / Swipe Length are only meaningful in Gesture
// mode, so they are hidden when Scroll Mode = Wheel. Toggling Scroll Mode posts
// a custom rebuild event back to the scene so the list can safely reset itself
// outside the change callback (resetting the list from within a change callback
// would tear down the very item being processed).

#define TIKTOK_SETTINGS_EVENT_REBUILD 0xFE

static void tiktok_scroll_mode_changed(VariableItem* item) {
    Hid* app = variable_item_get_context(item);
    uint8_t idx = variable_item_get_current_value_index(item);
    app->tiktok_scroll_mode = idx;
    variable_item_set_current_value_text(item, (idx == TikTokScrollGesture) ? "Gesture" : "Wheel");
    bt_remotes_save_profile_menu_cfg(app);
    view_dispatcher_send_custom_event(app->view_dispatcher, TIKTOK_SETTINGS_EVENT_REBUILD);
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

// Help is the last row; its index depends on whether the gesture rows are visible.
static inline uint8_t tiktok_help_row(const Hid* app) {
    return (app->tiktok_scroll_mode == TikTokScrollGesture) ? 4 : 1;
}

static void tiktok_settings_enter_cb(void* context, uint32_t index) {
    Hid* app = context;
    if(index == tiktok_help_row(app)) {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneRemoteSettingsHelp, RemoteSettingsHelpTikTok);
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRemoteSettingsHelp);
    }
}

static void build_tiktok_list(Hid* app) {
    VariableItemList* vil = app->var_item_list;
    variable_item_list_reset(vil);
    variable_item_list_set_header(vil, "TikTok Remote");

    VariableItem* item;
    char buf[8];

    // Scroll Mode (always present).
    item = variable_item_list_add(
        vil, "Scroll Mode", TIKTOK_SCROLL_MODE_COUNT, tiktok_scroll_mode_changed, app);
    variable_item_set_current_value_index(item, app->tiktok_scroll_mode);
    variable_item_set_current_value_text(
        item, (app->tiktok_scroll_mode == TikTokScrollGesture) ? "Gesture" : "Wheel");

    // Inward Margin / Edge Margin / Swipe Length are only meaningful in Gesture mode.
    if(app->tiktok_scroll_mode == TikTokScrollGesture) {
        item = variable_item_list_add(
            vil,
            "Inward Margin",
            BT_REMOTES_VALUE_COUNT(
                TIKTOK_GESTURE_INSET_MIN, TIKTOK_GESTURE_INSET_MAX, TIKTOK_GESTURE_INSET_STEP),
            tiktok_inset_changed,
            app);
        variable_item_set_current_value_index(
            item,
            (app->tiktok_gesture_inset - TIKTOK_GESTURE_INSET_MIN) / TIKTOK_GESTURE_INSET_STEP);
        snprintf(buf, sizeof(buf), "%u", (unsigned)app->tiktok_gesture_inset);
        variable_item_set_current_value_text(item, buf);

        item = variable_item_list_add(
            vil,
            "Edge Margin",
            BT_REMOTES_VALUE_COUNT(
                TIKTOK_GESTURE_MARGIN_MIN, TIKTOK_GESTURE_MARGIN_MAX, TIKTOK_GESTURE_MARGIN_STEP),
            tiktok_margin_changed,
            app);
        variable_item_set_current_value_index(
            item,
            (app->tiktok_gesture_margin - TIKTOK_GESTURE_MARGIN_MIN) / TIKTOK_GESTURE_MARGIN_STEP);
        snprintf(buf, sizeof(buf), "%u", (unsigned)app->tiktok_gesture_margin);
        variable_item_set_current_value_text(item, buf);

        item = variable_item_list_add(
            vil,
            "Swipe Length",
            BT_REMOTES_VALUE_COUNT(
                TIKTOK_GESTURE_SWIPE_MIN, TIKTOK_GESTURE_SWIPE_MAX, TIKTOK_GESTURE_SWIPE_STEP),
            tiktok_swipe_changed,
            app);
        variable_item_set_current_value_index(
            item,
            (app->tiktok_gesture_swipe - TIKTOK_GESTURE_SWIPE_MIN) / TIKTOK_GESTURE_SWIPE_STEP);
        snprintf(buf, sizeof(buf), "%u", (unsigned)app->tiktok_gesture_swipe);
        variable_item_set_current_value_text(item, buf);
    }

    // Help (no value; OK opens the help page via the enter callback).
    variable_item_list_add(vil, "Help", 1, NULL, app);

    variable_item_list_set_enter_callback(vil, tiktok_settings_enter_cb, app);
}

void bt_remotes_scene_tiktok_settings_on_enter(void* context) {
    Hid* app = context;
    build_tiktok_list(app);
    variable_item_list_set_selected_item(
        app->var_item_list,
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneTikTokSettings));
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewVariableItemList);
}

bool bt_remotes_scene_tiktok_settings_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == TIKTOK_SETTINGS_EVENT_REBUILD) {
        build_tiktok_list(app);
        // Toggling Scroll Mode is always done from row 0; keep selection there so
        // the user can flip back-and-forth without losing context.
        variable_item_list_set_selected_item(app->var_item_list, 0);
        return true;
    }
    return false;
}

void bt_remotes_scene_tiktok_settings_on_exit(void* context) {
    Hid* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        BtRemotesSceneTikTokSettings,
        variable_item_list_get_selected_item_index(app->var_item_list));
    variable_item_list_reset(app->var_item_list);
}
