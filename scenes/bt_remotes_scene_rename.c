#include "../bt_remotes.h"
#include "../views.h"
#include "hid_icons.h"

enum BtRemotesRenameEvent {
    BtRemotesRenameEventTextInput,
    BtRemotesRenameEventPopup,
};

static void bt_remotes_scene_rename_text_input_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtRemotesRenameEventTextInput);
}

static void bt_remotes_scene_rename_popup_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BtRemotesRenameEventPopup);
}

void bt_remotes_scene_rename_on_enter(void* context) {
    Hid* app = context;

    text_input_reset(app->text_input);
    text_input_set_result_callback(
        app->text_input,
        bt_remotes_scene_rename_text_input_cb,
        app,
        app->ble_hid_cfg.name,
        sizeof(app->ble_hid_cfg.name),
        true);
    text_input_set_header_text(app->text_input, "Bluetooth Name");

    popup_set_icon(app->popup, 48, 6, &I_DolphinDone_80x58);
    popup_set_header(app->popup, "Done", 14, 15, AlignLeft, AlignTop);
    popup_set_timeout(app->popup, 1500);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, bt_remotes_scene_rename_popup_cb);
    popup_enable_timeout(app->popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewTextInput);
}

bool bt_remotes_scene_rename_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == BtRemotesRenameEventTextInput) {
            furi_hal_bt_stop_advertising();
            app->ble_hid_profile =
                bt_profile_start(app->bt, ble_profile_hid_ext, &app->ble_hid_cfg);
            furi_check(app->ble_hid_profile);
            furi_hal_bt_start_advertising();

            bt_hid_save_cfg(app);

            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);
        } else if(event.event == BtRemotesRenameEventPopup) {
            scene_manager_previous_scene(app->scene_manager);
        }
    }

    return consumed;
}

void bt_remotes_scene_rename_on_exit(void* context) {
    Hid* app = context;
    text_input_reset(app->text_input);
    popup_reset(app->popup);
}
