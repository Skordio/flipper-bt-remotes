#include "../bt_remotes.h"

// The Settings entry is always visible — its index equals BT_REMOTES_MENU_ITEM_COUNT - 1.
#define HIDE_ITEMS_SETTINGS_IDX ((uint8_t)(BT_REMOTES_MENU_ITEM_COUNT - 1))

// File-scope label buffers — must outlive the submenu display (one slot per hideable item).
static char hide_labels[BT_REMOTES_MENU_ITEM_COUNT][40];

enum {
    BtRemotesHideItemsEventRebuild,
};

// ---------------------------------------------------------------------------
// Callback fired when any item is tapped in the submenu
// ---------------------------------------------------------------------------

static void hide_items_cb(void* context, uint32_t index) {
    Hid* app = context;
    // index is the BtRemotesStartIndex value of the tapped item
    app->menu_hidden ^= (1u << index);
    // Belt-and-suspenders: Settings must never be hidden
    app->menu_hidden &= ~(1u << HIDE_ITEMS_SETTINGS_IDX);
    bt_remotes_save_profile_menu_cfg(app);
    // Record which item was toggled so on_event can restore the cursor after rebuild
    scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneHideItems, index);
    view_dispatcher_send_custom_event(app->view_dispatcher, BtRemotesHideItemsEventRebuild);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Rebuild the submenu labels and wire callbacks.
// Items are shown in menu_order sequence; Settings is excluded (always visible).
static void build_submenu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Hide Remote Types");
    uint8_t slot = 0;
    for(uint8_t i = 0; i < BT_REMOTES_MENU_ORDER_LEN; i++) {
        uint8_t idx = app->menu_order[i];
        if(idx == 0xFF) continue;                        // empty sentinel
        if(idx >= BT_REMOTES_MENU_ITEM_COUNT) continue; // custom-remote slot — not hideable
        if(idx == HIDE_ITEMS_SETTINGS_IDX) continue;    // Settings is always shown
        bool hidden = (app->menu_hidden >> idx) & 1;
        // bt_remotes_menu_default[idx] is valid because index == enum value == array position
        snprintf(
            hide_labels[slot],
            sizeof(hide_labels[slot]),
            "%s %s",
            hidden ? "[ ]" : "[x]",
            bt_remotes_menu_default[idx].label);
        submenu_add_item(app->submenu, hide_labels[slot], idx, hide_items_cb, app);
        slot++;
    }
}

// Return the submenu position (0-based) of the item whose BtRemotesStartIndex == target_idx,
// so the cursor can be restored after a toggle+rebuild cycle.
static uint8_t find_submenu_pos(Hid* app, uint8_t target_idx) {
    uint8_t pos = 0;
    for(uint8_t i = 0; i < BT_REMOTES_MENU_ORDER_LEN; i++) {
        uint8_t idx = app->menu_order[i];
        if(idx == 0xFF) continue;
        if(idx >= BT_REMOTES_MENU_ITEM_COUNT) continue;
        if(idx == HIDE_ITEMS_SETTINGS_IDX) continue;
        if(idx == target_idx) return pos;
        pos++;
    }
    return 0; // fall back to top if not found
}

// ---------------------------------------------------------------------------
// Scene handlers
// ---------------------------------------------------------------------------

void bt_remotes_scene_hide_items_on_enter(void* context) {
    Hid* app = context;
    build_submenu(app);
    // Restore cursor: 0 on first entry, or last-toggled item's position on rebuild re-entry
    uint8_t last_idx = (uint8_t)scene_manager_get_scene_state(
        app->scene_manager, BtRemotesSceneHideItems);
    submenu_set_selected_item(app->submenu, find_submenu_pos(app, last_idx));
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_hide_items_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom &&
       event.event == BtRemotesHideItemsEventRebuild) {
        build_submenu(app);
        uint8_t last_idx = (uint8_t)scene_manager_get_scene_state(
            app->scene_manager, BtRemotesSceneHideItems);
        submenu_set_selected_item(app->submenu, find_submenu_pos(app, last_idx));
        consumed = true;
    }
    return consumed;
}

void bt_remotes_scene_hide_items_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
