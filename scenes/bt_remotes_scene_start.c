#include "../bt_remotes.h"
#include "../views.h"
#include "../views/hid_remote_menu.h"

// ---------------------------------------------------------------------------
// Static default item table — labels parallel to the enum values above
// ---------------------------------------------------------------------------

// Defined without 'static' so bt_remotes_scene_hide_items.c can reach it via extern in bt_remotes.h
const BtRemotesMenuEntry bt_remotes_menu_default[BT_REMOTES_MENU_ITEM_COUNT] = {
    {"Keynote",               BtRemotesStartIndexKeynote},
    {"Keynote Vertical",      BtRemotesStartIndexKeynoteVertical},
    {"Keyboard",              BtRemotesStartIndexKeyboard},
    {"Numpad",                BtRemotesStartIndexNumpad},
    {"Media",                 BtRemotesStartIndexMedia},
    {"Apple Music macOS",     BtRemotesStartIndexMusicMacOs},
    {"Movie",                 BtRemotesStartIndexMovie},
    {"TikTok / YT Shorts",    BtRemotesStartIndexTikTok},
    {"Mouse",                 BtRemotesStartIndexMouse},
    {"Mouse Clicker",         BtRemotesStartIndexMouseClicker},
    {"Mouse Jiggler",         BtRemotesStartIndexMouseJiggler},
    {"Mouse Jiggler Stealth", BtRemotesStartIndexMouseJigglerStealth},
    {"PushToTalk",            BtRemotesStartIndexPushToTalk},
    {"Ducky Scripts",         BtRemotesStartIndexCustomActions},
    {"Custom Gestures",       BtRemotesStartIndexCustomGestures},
    // Settings stays last so bt_remotes_menu_default[i].index == i (hide_items
    // relies on entry position == enum index, and several spots assume Settings
    // is the highest-indexed fixed item).
    {"Settings",              BtRemotesStartIndexSettings},
};

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------

static void bt_remotes_start_select_cb(void* context, uint8_t index_value) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index_value);
}

static void
    bt_remotes_start_reorder_cb(void* context, const uint8_t* new_order, uint8_t count) {
    Hid* app = context;
    for(uint8_t i = 0; i < BT_REMOTES_MENU_ORDER_LEN; i++) app->menu_order[i] = 0xFF;
    uint8_t k = 0;
    for(uint8_t i = 0; i < count && k < BT_REMOTES_MENU_ORDER_LEN; i++) {
        app->menu_order[k++] = new_order[i];
    }
    // Append hidden fixed items so they can be un-hidden later
    for(uint8_t idx = 0;
        idx < BT_REMOTES_MENU_ITEM_COUNT && k < BT_REMOTES_MENU_ORDER_LEN;
        idx++) {
        if(app->menu_hidden & (1u << idx)) {
            app->menu_order[k++] = idx;
        }
    }
    bt_remotes_save_profile_menu_cfg(app);
}

// ---------------------------------------------------------------------------
// Scene handlers
// ---------------------------------------------------------------------------

void bt_remotes_scene_start_on_enter(void* context) {
    Hid* app = context;

    // Delay-connect profiles disconnect whenever the Start menu is shown: BLE is
    // only up while a remote/script/gesture screen is open. First arrival is a
    // no-op (BLE already stopped); returning from a remote/Settings drops it.
    // Immediate-connect profiles ensure BLE is up at the menu — normally a no-op,
    // but it reconnects after a Ducky "connect per run" session dropped the link.
    if(app->delay_connect) {
        bt_remotes_stop_ble(app);
    } else if(!app->ble_started) {
        bt_remotes_start_ble(app);
    }

    // Max items: BT_REMOTES_MENU_ITEM_COUNT fixed + BT_REMOTES_PINNED_MAX pinned
    BtRemotesMenuEntry entries[BT_REMOTES_MENU_ORDER_LEN];
    uint8_t            order[BT_REMOTES_MENU_ORDER_LEN];
    uint8_t            total = 0;

    // Track which pinned collections are already placed via menu_order
    bool pinned_placed[BT_REMOTES_PINNED_MAX];
    for(uint8_t i = 0; i < BT_REMOTES_PINNED_MAX; i++) pinned_placed[i] = false;

    // Track which fixed items the saved menu_order already accounts for, so items
    // added in a firmware update (absent from old saved orders) can be appended.
    bool fixed_placed[BT_REMOTES_MENU_ITEM_COUNT];
    for(uint8_t i = 0; i < BT_REMOTES_MENU_ITEM_COUNT; i++) fixed_placed[i] = false;

    for(uint8_t i = 0; i < BT_REMOTES_MENU_ORDER_LEN && total < BT_REMOTES_MENU_ORDER_LEN; i++) {
        uint8_t slot = app->menu_order[i];
        if(slot == 0xFF) continue;

        if(slot < BT_REMOTES_MENU_ITEM_COUNT) {
            // Fixed built-in item
            fixed_placed[slot] = true; // accounted for (whether shown or hidden)
            if(app->menu_hidden & (1u << slot)) continue;
            entries[total] = bt_remotes_menu_default[slot];
            order[total]   = slot;
            total++;
        } else {
            // Pinned collection slot
            uint8_t pidx = slot - BT_REMOTES_MENU_ITEM_COUNT;
            if(pidx >= app->pinned_count) continue;
            entries[total].label = app->pinned_collections[pidx];
            entries[total].index = slot;
            order[total]         = slot;
            pinned_placed[pidx]  = true;
            total++;
        }
    }

    // Append any fixed items missing from the saved menu_order (e.g. added in a
    // firmware update, like Custom Gestures), so existing profiles get them
    // without needing "Reset Menu Order". Hidden items stay hidden.
    for(uint8_t idx = 0; idx < BT_REMOTES_MENU_ITEM_COUNT && total < BT_REMOTES_MENU_ORDER_LEN;
        idx++) {
        if(fixed_placed[idx]) continue;
        if(app->menu_hidden & (1u << idx)) continue;
        entries[total] = bt_remotes_menu_default[idx];
        order[total]   = idx;
        total++;
    }

    // Append any newly-pinned collections not yet tracked in menu_order
    for(uint8_t pidx = 0;
        pidx < app->pinned_count && total < BT_REMOTES_MENU_ORDER_LEN;
        pidx++) {
        if(!pinned_placed[pidx]) {
            uint8_t slot         = BT_REMOTES_MENU_ITEM_COUNT + pidx;
            entries[total].label = app->pinned_collections[pidx];
            entries[total].index = slot;
            order[total]         = slot;
            total++;
        }
    }

    hid_remote_menu_set_items(
        app->hid_remote_menu,
        entries,
        order,
        total,
        0); // all items fully reorderable
    hid_remote_menu_set_select_callback(app->hid_remote_menu, bt_remotes_start_select_cb, app);
    hid_remote_menu_set_reorder_callback(
        app->hid_remote_menu, bt_remotes_start_reorder_cb, app);

    hid_remote_menu_set_selected_index(
        app->hid_remote_menu,
        (uint8_t)scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewRemoteMenu);
}

bool bt_remotes_scene_start_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        bt_remotes_stop_ble(app);
        app->active_profile[0] = '\0';
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, BtRemotesSceneProfileSelect);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        // Delay-connect: any selection except Settings is a HID destination
        // (remote view, Ducky/Gesture list, or pinned script/gesture) — bring BLE
        // up now. Settings must stay disconnected and stops BLE in its own branch.
        // Bound to real menu events so a stray high-value event (e.g. a connect-wait
        // tick that arrived after the run scene closed) can't spuriously start BLE.
        if(app->delay_connect && !app->ble_started &&
           event.event != BtRemotesStartIndexSettings &&
           event.event < BT_REMOTES_MENU_ORDER_LEN) {
            // "Connect per run" overrides this for Ducky/Collection launches: they
            // stay disconnected here and connect only inside the run scene. A
            // Ducky/Collection target is the Ducky list or a pinned collection
            // (kind 0). Gestures (list or pinned kind 1) are unaffected.
            bool ducky_target = (event.event == BtRemotesStartIndexCustomActions);
            if(event.event >= BT_REMOTES_MENU_ITEM_COUNT) {
                uint8_t pidx = event.event - BT_REMOTES_MENU_ITEM_COUNT;
                if(pidx < app->pinned_count && app->pinned_kinds[pidx] == 0) ducky_target = true;
            }
            if(!(app->ducky_connect_per_run && ducky_target)) bt_remotes_start_ble(app);
        }
        if(event.event == BtRemotesStartIndexSettings) {
            scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneStart, event.event);
            bt_remotes_stop_ble(app);
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileSettings);
            consumed = true;
        } else if(event.event == BtRemotesStartIndexCustomActions) {
            scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneStart, event.event);
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCustomActions);
            consumed = true;
        } else if(event.event == BtRemotesStartIndexCustomGestures) {
            scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneStart, event.event);
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneGestureList);
            consumed = true;
        } else if(event.event >= BT_REMOTES_MENU_ITEM_COUNT) {
            // Pinned slot — route by kind (collection viewer vs gesture runner).
            uint8_t pidx = event.event - BT_REMOTES_MENU_ITEM_COUNT;
            if(pidx < app->pinned_count) {
                scene_manager_set_scene_state(
                    app->scene_manager, BtRemotesSceneStart, event.event);
                if(app->pinned_kinds[pidx] == 1) {
                    bt_remotes_gesture_path(
                        app->pinned_collections[pidx],
                        app->pending_script_path,
                        sizeof(app->pending_script_path));
                    scene_manager_next_scene(app->scene_manager, BtRemotesSceneGestureRun);
                } else {
                    bt_remotes_collection_load(app, app->pinned_collections[pidx]);
                    scene_manager_next_scene(app->scene_manager, BtRemotesSceneCollectionView);
                }
                consumed = true;
            }
        } else {
            scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneStart, event.event);
            HidView view_id;

            switch(event.event) {
            case BtRemotesStartIndexKeynote:
                view_id = HidViewKeynote;
                hid_keynote_set_orientation(app->hid_keynote, false);
                break;
            case BtRemotesStartIndexKeynoteVertical:
                view_id = HidViewKeynote;
                hid_keynote_set_orientation(app->hid_keynote, true);
                break;
            case BtRemotesStartIndexKeyboard:
                view_id = HidViewKeyboard;
                break;
            case BtRemotesStartIndexNumpad:
                view_id = HidViewNumpad;
                break;
            case BtRemotesStartIndexMedia:
                view_id = HidViewMedia;
                hid_media_set_mode(
                    app->hid_media,
                    app->media_mode == MediaModeImproved,
                    app->media_mouse_switch);
                break;
            case BtRemotesStartIndexMusicMacOs:
                view_id = HidViewMusicMacOs;
                break;
            case BtRemotesStartIndexMovie:
                view_id = HidViewMovie;
                break;
            case BtRemotesStartIndexTikTok:
                view_id = BtHidViewTikTok;
                hid_tiktok_set_mode(
                    app->hid_tiktok, app->tiktok_scroll_mode == TikTokScrollGesture);
                break;
            case BtRemotesStartIndexMouse:
                view_id = HidViewMouse;
                // Standalone Mouse remote: short Back = right-click (reset the shared
                // instance in case the Media remote left it in return-to-Media mode)
                hid_mouse_set_back_to_view(app->hid_mouse, false, 0);
                break;
            case BtRemotesStartIndexMouseClicker:
                view_id = HidViewMouseClicker;
                break;
            case BtRemotesStartIndexMouseJiggler:
                view_id = HidViewMouseJiggler;
                break;
            case BtRemotesStartIndexMouseJigglerStealth:
                view_id = HidViewMouseJigglerStealth;
                break;
            case BtRemotesStartIndexPushToTalk:
                view_id = HidViewPushToTalkMenu;
                break;
            default:
                furi_crash();
            }

            scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneMain, view_id);
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneMain);
            consumed = true;
        }
    }

    return consumed;
}

void bt_remotes_scene_start_on_exit(void* context) {
    UNUSED(context);
}
