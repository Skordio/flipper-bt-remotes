#include "../bt_remotes.h"

// Settings entry is never shown here (it doesn't enter the Main remote scene).
#define KB_MODE_SETTINGS_IDX ((uint8_t)(BtRemotesStartIndexSettings))

static char kb_labels[BT_REMOTES_MENU_ITEM_COUNT][48];

static void kb_mode_cb(void* context, uint32_t index) {
    Hid* app = context;
    app->remote_kb_suppress ^= (uint16_t)(1u << index);
    bt_remotes_save_profile_menu_cfg(app);
    scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneKbMode, index);
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

static void build_kb_submenu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "iOS Keyboard Mode");
    uint8_t slot = 0;
    for(uint8_t i = 0; i < BT_REMOTES_MENU_ORDER_LEN; i++) {
        uint8_t idx = app->menu_order[i];
        if(idx == 0xFF) continue;
        if(idx >= BT_REMOTES_MENU_ITEM_COUNT) continue; // pinned collection slot
        if(idx == KB_MODE_SETTINGS_IDX) continue;       // Settings doesn't enter Main
        bool suppresses = (app->remote_kb_suppress >> idx) & 1u;
        snprintf(
            kb_labels[slot],
            sizeof(kb_labels[slot]),
            "%s %s",
            suppresses ? "[Hides]" : "[Shows]",
            bt_remotes_menu_default[idx].label);
        submenu_add_item(app->submenu, kb_labels[slot], idx, kb_mode_cb, app);
        slot++;
    }
}

void bt_remotes_scene_kb_mode_on_enter(void* context) {
    Hid* app = context;
    build_kb_submenu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_kb_mode_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        build_kb_submenu(app);
        uint8_t last_idx = (uint8_t)scene_manager_get_scene_state(
            app->scene_manager, BtRemotesSceneKbMode);
        submenu_set_selected_item(app->submenu, last_idx);
        consumed = true;
    }
    return consumed;
}

void bt_remotes_scene_kb_mode_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
