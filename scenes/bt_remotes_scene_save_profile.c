#include "../bt_remotes.h"

enum BtRemotesSaveProfileEvent {
    BtRemotesSaveProfileEventDone,
};

static void bt_remotes_scene_save_profile_popup_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtRemotesSaveProfileEventDone);
}

void bt_remotes_scene_save_profile_on_enter(void* context) {
    Hid* app = context;

    popup_reset(app->popup);
    if(bt_remotes_profile_save(app)) {
        popup_set_header(app->popup, "Saved!", 64, 10, AlignCenter, AlignTop);
        popup_set_text(app->popup, app->active_profile, 64, 28, AlignCenter, AlignTop);
        popup_set_timeout(app->popup, 1500);
    } else {
        popup_set_header(app->popup, "Error", 64, 10, AlignCenter, AlignTop);
        popup_set_text(
            app->popup,
            "No pairing data.\nPair first.",
            64,
            28,
            AlignCenter,
            AlignTop);
        popup_set_timeout(app->popup, 2500);
    }
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, bt_remotes_scene_save_profile_popup_cb);
    popup_enable_timeout(app->popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);
}

bool bt_remotes_scene_save_profile_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtRemotesSaveProfileEventDone) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void bt_remotes_scene_save_profile_on_exit(void* context) {
    Hid* app = context;
    popup_reset(app->popup);
}
