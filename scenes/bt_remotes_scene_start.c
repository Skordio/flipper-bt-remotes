#include "../bt_remotes.h"
#include "../views.h"
#include "../views/hid_remote_menu.h"

// ---------------------------------------------------------------------------
// Item index enum — these are the "values" carried through event dispatch
// ---------------------------------------------------------------------------

enum BtRemotesStartIndex {
    BtRemotesStartIndexKeynote,
    BtRemotesStartIndexKeynoteVertical,
    BtRemotesStartIndexKeyboard,
    BtRemotesStartIndexNumpad,
    BtRemotesStartIndexMedia,
    BtRemotesStartIndexMusicMacOs,
    BtRemotesStartIndexMovie,
    BtRemotesStartIndexTikTok,
    BtRemotesStartIndexMouse,
    BtRemotesStartIndexMouseClicker,
    BtRemotesStartIndexMouseJiggler,
    BtRemotesStartIndexMouseJigglerStealth,
    BtRemotesStartIndexPushToTalk,
    BtRemotesStartIndexSettings,
};

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
    // Rebuild menu_order: visible items in their new order first, then hidden items appended
    uint8_t k = 0;
    for(uint8_t i = 0; i < count && k < BT_REMOTES_MENU_ITEM_COUNT; i++) {
        app->menu_order[k++] = new_order[i];
    }
    // Append hidden items in natural enum order so they're easy to find later
    for(uint8_t idx = 0;
        idx < BT_REMOTES_MENU_ITEM_COUNT && k < BT_REMOTES_MENU_ITEM_COUNT;
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

    // Build the visible item list by walking menu_order and skipping hidden items.
    // bt_remotes_menu_default[idx].label is valid for idx in [0, BT_REMOTES_MENU_ITEM_COUNT).
    BtRemotesMenuEntry visible_entries[BT_REMOTES_MENU_ITEM_COUNT];
    uint8_t            visible_order[BT_REMOTES_MENU_ITEM_COUNT];
    uint8_t            visible_count = 0;

    for(uint8_t i = 0; i < BT_REMOTES_MENU_ITEM_COUNT; i++) {
        uint8_t idx = app->menu_order[i];
        if(idx >= BT_REMOTES_MENU_ITEM_COUNT) continue;    // safety
        if(app->menu_hidden & (1u << idx)) continue;        // skip hidden
        visible_entries[visible_count].index = idx;
        visible_entries[visible_count].label = bt_remotes_menu_default[idx].label;
        visible_order[visible_count]         = idx;
        visible_count++;
    }

    hid_remote_menu_set_items(
        app->hid_remote_menu,
        visible_entries,
        visible_order,
        visible_count,
        0); // all visible items are reorderable
    hid_remote_menu_set_select_callback(app->hid_remote_menu, bt_remotes_start_select_cb, app);
    hid_remote_menu_set_reorder_callback(
        app->hid_remote_menu, bt_remotes_start_reorder_cb, app);

    // Restore cursor to the last item the user interacted with
    hid_remote_menu_set_selected_index(
        app->hid_remote_menu,
        (uint8_t)scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewRemoteMenu);
}

bool bt_remotes_scene_start_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Stop BLE, clear the active profile, and return to profile select
        bt_remotes_stop_ble(app);
        app->active_profile[0] = '\0';
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, BtRemotesSceneProfileSelect);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneStart, event.event);
        consumed = true;

        if(event.event == BtRemotesStartIndexSettings) {
            bt_remotes_stop_ble(app);
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneSettings);
        } else {
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
                break;
            case BtRemotesStartIndexMusicMacOs:
                view_id = HidViewMusicMacOs;
                break;
            case BtRemotesStartIndexMovie:
                view_id = HidViewMovie;
                break;
            case BtRemotesStartIndexTikTok:
                view_id = BtHidViewTikTok;
                break;
            case BtRemotesStartIndexMouse:
                view_id = HidViewMouse;
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
        }
    }

    return consumed;
}

void bt_remotes_scene_start_on_exit(void* context) {
    UNUSED(context);
    // HidRemoteMenu has no reset needed
}
