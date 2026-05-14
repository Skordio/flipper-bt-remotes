#include "../bt_remotes.h"

enum BtRemotesProfileRenameFileEvent {
    BtRemotesProfileRenameFileEventDone,
    BtRemotesProfileRenameFileEventPopup,
};

static void bt_remotes_scene_profile_rename_file_text_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, BtRemotesProfileRenameFileEventDone);
}

static void bt_remotes_scene_profile_rename_file_popup_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, BtRemotesProfileRenameFileEventPopup);
}

static bool bt_remotes_scene_profile_rename_file_validator(
    const char* text,
    FuriString* error,
    void* context) {
    Hid* app = context;

    if(text[0] == '\0') {
        furi_string_set(error, "Name cannot\nbe empty");
        return false;
    }

    const char* invalid = "<>:\"/\\|?*";
    for(size_t i = 0; text[i]; i++) {
        if(strchr(invalid, text[i])) {
            furi_string_printf(error, "Char '%c' not\nallowed", text[i]);
            return false;
        }
    }

    // Allow keeping the same name (no collision check against self)
    if(strcmp(text, app->pending_name) != 0) {
        FuriString* path = furi_string_alloc_printf(
            "%s/%s%s", BT_REMOTES_PROFILES_DIR, text, BT_REMOTES_CFG_EXT);
        bool exists = storage_file_exists(app->storage, furi_string_get_cstr(path));
        furi_string_free(path);
        if(exists) {
            furi_string_set(error, "Name already\nin use");
            return false;
        }
    }

    return true;
}

void bt_remotes_scene_profile_rename_file_on_enter(void* context) {
    Hid* app = context;

    // Save old name so we know which files to rename
    strlcpy(app->pending_name, app->active_profile, BT_REMOTES_PROFILE_NAME_LEN);

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Rename Profile");
    text_input_set_result_callback(
        app->text_input,
        bt_remotes_scene_profile_rename_file_text_cb,
        app,
        app->active_profile,
        BT_REMOTES_PROFILE_NAME_LEN,
        true);
    text_input_set_validator(
        app->text_input, bt_remotes_scene_profile_rename_file_validator, app);

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewTextInput);
}

bool bt_remotes_scene_profile_rename_file_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == BtRemotesProfileRenameFileEventDone) {
            popup_reset(app->popup);
            if(bt_remotes_profile_rename(app)) {
                popup_set_header(app->popup, "Renamed!", 64, 10, AlignCenter, AlignTop);
                popup_set_text(
                    app->popup, app->active_profile, 64, 28, AlignCenter, AlignTop);
                popup_set_timeout(app->popup, 1500);
            } else {
                popup_set_header(app->popup, "Error", 64, 10, AlignCenter, AlignTop);
                popup_set_text(
                    app->popup, "Rename failed.", 64, 28, AlignCenter, AlignTop);
                popup_set_timeout(app->popup, 2000);
                // Restore old name on failure
                strlcpy(app->active_profile, app->pending_name, BT_REMOTES_PROFILE_NAME_LEN);
            }
            popup_set_context(app->popup, app);
            popup_set_callback(
                app->popup, bt_remotes_scene_profile_rename_file_popup_cb);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);

        } else if(event.event == BtRemotesProfileRenameFileEventPopup) {
            scene_manager_previous_scene(app->scene_manager);
        }
    }

    return consumed;
}

void bt_remotes_scene_profile_rename_file_on_exit(void* context) {
    Hid* app = context;
    text_input_reset(app->text_input);
    popup_reset(app->popup);
}
