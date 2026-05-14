#include "../bt_remotes.h"
#include "../views.h"

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
    BtRemotesStartIndexRemovePairing,
    BtRemotesStartIndexSaveProfile,
    BtRemotesStartIndexSwitchProfile,
    BtRemotesStartIndexDeleteProfile,
};

static void bt_remotes_scene_start_submenu_callback(void* context, uint32_t index) {
    furi_assert(context);
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bt_remotes_scene_start_on_enter(void* context) {
    Hid* app = context;
    submenu_add_item(
        app->submenu,
        "Keynote",
        BtRemotesStartIndexKeynote,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Keynote Vertical",
        BtRemotesStartIndexKeynoteVertical,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Keyboard",
        BtRemotesStartIndexKeyboard,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Numpad",
        BtRemotesStartIndexNumpad,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Media",
        BtRemotesStartIndexMedia,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Apple Music macOS",
        BtRemotesStartIndexMusicMacOs,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Movie",
        BtRemotesStartIndexMovie,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Mouse",
        BtRemotesStartIndexMouse,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "TikTok / YT Shorts",
        BtRemotesStartIndexTikTok,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Mouse Clicker",
        BtRemotesStartIndexMouseClicker,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Mouse Jiggler",
        BtRemotesStartIndexMouseJiggler,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Mouse Jiggler Stealth",
        BtRemotesStartIndexMouseJigglerStealth,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "PushToTalk",
        BtRemotesStartIndexPushToTalk,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Settings",
        BtRemotesStartIndexSettings,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Bluetooth Unpairing",
        BtRemotesStartIndexRemovePairing,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Save Profile",
        BtRemotesStartIndexSaveProfile,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Switch Profile",
        BtRemotesStartIndexSwitchProfile,
        bt_remotes_scene_start_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Delete Profile",
        BtRemotesStartIndexDeleteProfile,
        bt_remotes_scene_start_submenu_callback,
        app);

    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneStart));
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_start_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Exit the app rather than looping back to profile select
        view_dispatcher_stop(app->view_dispatcher);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneStart, event.event);
        consumed = true;

        if(event.event == BtRemotesStartIndexRemovePairing) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneUnpair);
        } else if(event.event == BtRemotesStartIndexSettings) {
            bt_remotes_stop_ble(app);
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneSettings);
        } else if(event.event == BtRemotesStartIndexSaveProfile) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneSaveProfile);
        } else if(event.event == BtRemotesStartIndexSwitchProfile) {
            bt_remotes_stop_ble(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, BtRemotesSceneProfileSelect);
        } else if(event.event == BtRemotesStartIndexDeleteProfile) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneDeleteProfile);
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

            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneMain, view_id);
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneMain);
        }
    }

    return consumed;
}

void bt_remotes_scene_start_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
