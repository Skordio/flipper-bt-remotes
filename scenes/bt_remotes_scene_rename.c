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
    bool has_profile = app->active_profile[0] != '\0';

    text_input_reset(app->text_input);
    text_input_set_header_text(
        app->text_input, has_profile ? "Bluetooth Name" : "Default BT Name");
    text_input_set_result_callback(
        app->text_input,
        bt_remotes_scene_rename_text_input_cb,
        app,
        has_profile ? app->ble_hid_cfg.name : app->default_ble_name,
        has_profile ? sizeof(app->ble_hid_cfg.name) : sizeof(app->default_ble_name),
        true);

    popup_reset(app->popup);
    popup_set_icon(app->popup, 48, 6, &I_DolphinDone_80x58);
    popup_set_header(app->popup, "Saved!", 14, 15, AlignLeft, AlignTop);
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
            if(app->active_profile[0] != '\0') {
                // Profile active: persist name to active cfg and to profile's own cfg.
                // BLE was stopped when entering Settings; it will be restarted on
                // Back from Settings (via settings_on_event), loading the updated cfg.
                bt_hid_save_cfg(app);

                // Also write the updated cfg directly into the profile's .cfg file so
                // that bt_remotes_profile_activate (called on Back) restores the new
                // name rather than the old one.
                FuriString* prof_cfg = furi_string_alloc_printf(
                    "%s/%s%s",
                    BT_REMOTES_PROFILES_DIR,
                    app->active_profile,
                    BT_REMOTES_CFG_EXT);
                storage_common_remove(app->storage, furi_string_get_cstr(prof_cfg));
                storage_common_copy(
                    app->storage, BT_REMOTES_CFG_PATH, furi_string_get_cstr(prof_cfg));
                furi_string_free(prof_cfg);
            } else {
                // No profile selected: save as the new default name for future profiles.
                bt_remotes_save_app_cfg(app);
            }

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
