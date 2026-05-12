#include "../bt_remotes.h"
#include "../views.h"
#include "hid_icons.h"

enum BtRemotesDeleteProfileEvent {
    BtRemotesDeleteProfileEventConfirmed,
    BtRemotesDeleteProfileEventDone,
};

static void bt_remotes_scene_delete_profile_dialog_cb(DialogExResult result, void* context) {
    Hid* app = context;
    if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BtRemotesDeleteProfileEventConfirmed);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}

static void bt_remotes_scene_delete_profile_popup_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtRemotesDeleteProfileEventDone);
}

void bt_remotes_scene_delete_profile_on_enter(void* context) {
    Hid* app = context;

    dialog_ex_reset(app->dialog);
    dialog_ex_set_result_callback(app->dialog, bt_remotes_scene_delete_profile_dialog_cb);
    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_header(app->dialog, "Delete Profile?", 64, 3, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog, app->active_profile, 64, 22, AlignCenter, AlignTop);
    dialog_ex_set_left_button_text(app->dialog, "Back");
    dialog_ex_set_right_button_text(app->dialog, "Delete");

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewDialog);
}

bool bt_remotes_scene_delete_profile_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == BtRemotesDeleteProfileEventConfirmed) {
            bt_remotes_profile_delete(app);

            popup_reset(app->popup);
            popup_set_icon(app->popup, 48, 6, &I_DolphinDone_80x58);
            popup_set_header(app->popup, "Deleted", 14, 15, AlignLeft, AlignTop);
            popup_set_timeout(app->popup, 1500);
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, bt_remotes_scene_delete_profile_popup_cb);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);

        } else if(event.event == BtRemotesDeleteProfileEventDone) {
            // Profile deleted — stop BLE and go back to profile selection
            bt_remotes_stop_ble(app);
            app->active_profile[0] = '\0';
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, BtRemotesSceneProfileSelect);
        }
    }

    return consumed;
}

void bt_remotes_scene_delete_profile_on_exit(void* context) {
    Hid* app = context;
    dialog_ex_reset(app->dialog);
    popup_reset(app->popup);
}
