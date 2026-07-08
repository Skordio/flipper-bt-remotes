#pragma once

#include <gui/view.h>
#include <stdint.h>

// Capacity of the menu's internal item arrays. Must be at least
// BT_REMOTES_MENU_ORDER_LEN (fixed items + pin slots) — a static assert in
// bt_remotes_scene_start.c enforces this, since hid_remote_menu_set_items
// clamps to this capacity and would otherwise silently drop trailing items.
#define REMOTE_MENU_MAX_ITEMS 33 // 17 fixed items + 16 pinned collection/gesture slots

// Forward declarations
typedef struct HidRemoteMenu HidRemoteMenu;

// One entry in the static item table
typedef struct {
    const char* label;
    uint8_t     index; // BtRemotesStartIndex enum value
} BtRemotesMenuEntry;

// Callback fired when the user short-presses OK on an item (not in reorder mode)
typedef void (*HidRemoteMenuSelectCallback)(void* context, uint8_t index_value);

// Callback fired when reorder mode exits — passes the new order array and its length
typedef void (*HidRemoteMenuReorderCallback)(
    void* context,
    const uint8_t* new_order,
    uint8_t count);

HidRemoteMenu* hid_remote_menu_alloc(void);
void           hid_remote_menu_free(HidRemoteMenu* menu);
View*          hid_remote_menu_get_view(HidRemoteMenu* menu);

// Load item list from the default table + persistent order array.
// fixed_count items at the END of the list are never reorderable and are
// visually separated by a divider line.  Pass 0 for a fully reorderable list.
// Must be called before the view is shown.
void hid_remote_menu_set_items(
    HidRemoteMenu*            menu,
    const BtRemotesMenuEntry* default_table,
    const uint8_t*            order,
    uint8_t                   count,
    uint8_t                   fixed_count);

// Position the cursor at the visual row whose index_value matches the given value.
// If not found, cursor stays at 0.
void hid_remote_menu_set_selected_index(HidRemoteMenu* menu, uint8_t index_value);

// Returns the index_value of the item currently under the cursor (the user-
// provided BtRemotesStartIndex / profile-list-index, NOT the visual position).
// Returns 0xFF if the menu is empty.
uint8_t hid_remote_menu_get_selected_index(const HidRemoteMenu* menu);

void hid_remote_menu_set_select_callback(
    HidRemoteMenu*            menu,
    HidRemoteMenuSelectCallback cb,
    void*                     context);

void hid_remote_menu_set_reorder_callback(
    HidRemoteMenu*             menu,
    HidRemoteMenuReorderCallback cb,
    void*                      context);

// Mark one item as the visibility divider (identified by its index value).
// In normal (non-reorder) mode only items up to and including the divider
// item are shown and navigable.  Items after it are "hidden" and only
// appear when reorder mode is active.  A separator line is drawn below the
// divider item in reorder mode.
// Pass 0xFF to disable (show all items in both modes — the default).
void hid_remote_menu_set_divider(HidRemoteMenu* menu, uint8_t divider_value);
