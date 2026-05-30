#include "../bt_remotes.h"

enum BtRemotesTikTokSettingsIndex {
    BtRemotesTikTokSettingsIndexScrollMode = 0,
};

static char tiktok_labels[1][28];

static void tiktok_settings_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void build_tiktok_submenu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "TikTok Remote");

    snprintf(
        tiktok_labels[BtRemotesTikTokSettingsIndexScrollMode],
        sizeof(tiktok_labels[BtRemotesTikTokSettingsIndexScrollMode]),
        "Scroll Mode: %s",
        (app->tiktok_scroll_mode == TikTokScrollGesture) ? "Gesture" : "Wheel");
    submenu_add_item(
        app->submenu,
        tiktok_labels[BtRemotesTikTokSettingsIndexScrollMode],
        BtRemotesTikTokSettingsIndexScrollMode,
        tiktok_settings_cb,
        app);
}

void bt_remotes_scene_tiktok_settings_on_enter(void* context) {
    Hid* app = context;
    build_tiktok_submenu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_tiktok_settings_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtRemotesTikTokSettingsIndexScrollMode) {
            app->tiktok_scroll_mode = (app->tiktok_scroll_mode + 1) % TIKTOK_SCROLL_MODE_COUNT;
            bt_remotes_save_profile_menu_cfg(app);
            build_tiktok_submenu(app);
            submenu_set_selected_item(app->submenu, BtRemotesTikTokSettingsIndexScrollMode);
            consumed = true;
        }
    }

    return consumed;
}

void bt_remotes_scene_tiktok_settings_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
