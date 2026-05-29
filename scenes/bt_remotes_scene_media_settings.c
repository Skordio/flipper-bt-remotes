#include "../bt_remotes.h"

enum BtRemotesMediaSettingsIndex {
    BtRemotesMediaSettingsIndexMode        = 0,
    BtRemotesMediaSettingsIndexMouseSwitch = 1,
};

static char media_labels[2][28];

static void media_settings_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void build_media_submenu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Media Remote");

    snprintf(
        media_labels[BtRemotesMediaSettingsIndexMode],
        sizeof(media_labels[BtRemotesMediaSettingsIndexMode]),
        "Mode: %s",
        (app->media_mode == MediaModeImproved) ? "Improved" : "Legacy");
    submenu_add_item(
        app->submenu,
        media_labels[BtRemotesMediaSettingsIndexMode],
        BtRemotesMediaSettingsIndexMode,
        media_settings_cb,
        app);

    snprintf(
        media_labels[BtRemotesMediaSettingsIndexMouseSwitch],
        sizeof(media_labels[BtRemotesMediaSettingsIndexMouseSwitch]),
        "Mouse Switcher: %s",
        app->media_mouse_switch ? "On" : "Off");
    submenu_add_item(
        app->submenu,
        media_labels[BtRemotesMediaSettingsIndexMouseSwitch],
        BtRemotesMediaSettingsIndexMouseSwitch,
        media_settings_cb,
        app);
}

void bt_remotes_scene_media_settings_on_enter(void* context) {
    Hid* app = context;
    build_media_submenu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_media_settings_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtRemotesMediaSettingsIndexMode) {
            app->media_mode = (app->media_mode + 1) % MEDIA_MODE_COUNT;
            bt_remotes_save_profile_menu_cfg(app);
            build_media_submenu(app);
            submenu_set_selected_item(app->submenu, BtRemotesMediaSettingsIndexMode);
            consumed = true;
        } else if(event.event == BtRemotesMediaSettingsIndexMouseSwitch) {
            app->media_mouse_switch = app->media_mouse_switch ? 0 : 1;
            bt_remotes_save_profile_menu_cfg(app);
            build_media_submenu(app);
            submenu_set_selected_item(app->submenu, BtRemotesMediaSettingsIndexMouseSwitch);
            consumed = true;
        }
    }

    return consumed;
}

void bt_remotes_scene_media_settings_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
