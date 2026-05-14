#include "../bt_remotes.h"
#include "../views.h"
#include "hid_icons.h"

enum BtRemotesResetProfileEvent {
    BtRemotesResetProfileEventConfirmed,
    BtRemotesResetProfileEventDone,
};

static void bt_remotes_scene_reset_profile_dialog_cb(DialogExResult result, void* context) {
    Hid* app = context;
    if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, BtRemotesResetProfileEventConfirmed);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}

static void bt_remotes_scene_reset_profile_popup_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtRemotesResetProfileEventDone);
}

void bt_remotes_scene_reset_profile_on_enter(void* context) {
    Hid* app = context;

    dialog_ex_reset(app->dialog);
    dialog_ex_set_result_callback(app->dialog, bt_remotes_scene_reset_profile_dialog_cb);
    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_header(app->dialog, "Reset Profile?", 64, 3, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog,
        "New MAC, pairings\nwill be lost!",
        64,
        22,
        AlignCenter,
        AlignTop);
    dialog_ex_set_left_button_text(app->dialog, "Back");
    dialog_ex_set_right_button_text(app->dialog, "Reset");

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewDialog);
}

bool bt_remotes_scene_reset_profile_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == BtRemotesResetProfileEventConfirmed) {
            bt_remotes_stop_ble(app);
            bool ok = bt_remotes_profile_reset(app);
            if(ok) {
                bt_remotes_profile_activate(app);
                bt_remotes_start_ble(app);
            }

            popup_reset(app->popup);
            popup_set_icon(app->popup, 48, 6, &I_DolphinDone_80x58);
            if(ok) {
                popup_set_header(app->popup, "Reset!", 14, 15, AlignLeft, AlignTop);
                popup_set_text(
                    app->popup,
                    "Re-pair your device.",
                    14,
                    26,
                    AlignLeft,
                    AlignTop);
            } else {
                popup_set_header(app->popup, "Error", 14, 15, AlignLeft, AlignTop);
                popup_set_text(app->popup, "Reset failed.", 14, 26, AlignLeft, AlignTop);
            }
            popup_set_timeout(app->popup, 2000);
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, bt_remotes_scene_reset_profile_popup_cb);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);

        } else if(event.event == BtRemotesResetProfileEventDone) {
            scene_manager_previous_scene(app->scene_manager);
        }
    }

    return consumed;
}

void bt_remotes_scene_reset_profile_on_exit(void* context) {
    Hid* app = context;
    dialog_ex_reset(app->dialog);
    popup_reset(app->popup);
}
