#include "../bt_remotes.h"

static const char* const keynote_back_key_labels[KEYNOTE_BACK_KEY_COUNT] = {
    "Delete",
    "Left Arrow",
    "Escape",
    "None",
};

static void keynote_settings_cb(void* context, uint32_t index) {
    Hid* app = context;
    app->keynote_back_key = (uint8_t)index;
    bt_remotes_save_profile_menu_cfg(app);
    scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneKeynoteSettings, index);
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

static char keynote_labels[KEYNOTE_BACK_KEY_COUNT][20];

static void build_keynote_submenu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Keynote Back Button");
    for(uint8_t i = 0; i < KEYNOTE_BACK_KEY_COUNT; i++) {
        snprintf(
            keynote_labels[i],
            sizeof(keynote_labels[i]),
            "%s%s",
            (app->keynote_back_key == i) ? "\x10 " : "  ",
            keynote_back_key_labels[i]);
        submenu_add_item(app->submenu, keynote_labels[i], i, keynote_settings_cb, app);
    }
}

void bt_remotes_scene_keynote_settings_on_enter(void* context) {
    Hid* app = context;
    build_keynote_submenu(app);
    submenu_set_selected_item(app->submenu, app->keynote_back_key);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_keynote_settings_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        build_keynote_submenu(app);
        uint8_t selected = (uint8_t)scene_manager_get_scene_state(
            app->scene_manager, BtRemotesSceneKeynoteSettings);
        submenu_set_selected_item(app->submenu, selected);
        consumed = true;
    }
    return consumed;
}

void bt_remotes_scene_keynote_settings_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
