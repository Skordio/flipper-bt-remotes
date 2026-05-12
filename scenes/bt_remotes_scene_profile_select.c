#include "../bt_remotes.h"

#define NEW_PROFILE_INDEX UINT32_MAX

enum BtRemotesProfileSelectEvent {
    BtRemotesProfileSelectEventChosen,
    BtRemotesProfileSelectEventNew,
    BtRemotesProfileSelectEventAutoAdvance,
};

static void bt_remotes_scene_profile_select_cb(void* context, uint32_t index) {
    Hid* app = context;
    if(index == NEW_PROFILE_INDEX) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BtRemotesProfileSelectEventNew);
    } else {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneProfileSelect, index);
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BtRemotesProfileSelectEventChosen);
    }
}

void bt_remotes_scene_profile_select_on_enter(void* context) {
    Hid* app = context;

    // BLE already running means we arrived here after a profile was just created
    // (profile_new popped back). Skip straight to Start via an event to avoid
    // calling scene_manager_next_scene from within on_enter.
    if(app->ble_started) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BtRemotesProfileSelectEventAutoAdvance);
        return;
    }

    bt_remotes_profile_load_list(app);
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "BT Remotes");

    for(uint8_t i = 0; i < app->profile_count; i++) {
        submenu_add_item(
            app->submenu,
            app->profile_list[i],
            i,
            bt_remotes_scene_profile_select_cb,
            app);
    }
    submenu_add_item(
        app->submenu,
        "+ New Profile",
        NEW_PROFILE_INDEX,
        bt_remotes_scene_profile_select_cb,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_profile_select_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == BtRemotesProfileSelectEventAutoAdvance) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneStart);

        } else if(event.event == BtRemotesProfileSelectEventChosen) {
            uint32_t idx = scene_manager_get_scene_state(
                app->scene_manager, BtRemotesSceneProfileSelect);
            strlcpy(
                app->active_profile, app->profile_list[idx], BT_REMOTES_PROFILE_NAME_LEN);

            bt_remotes_profile_activate(app);
            bt_remotes_start_ble(app);

            scene_manager_next_scene(app->scene_manager, BtRemotesSceneStart);
        } else if(event.event == BtRemotesProfileSelectEventNew) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileNew);
        }
    }

    return consumed;
}

void bt_remotes_scene_profile_select_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
