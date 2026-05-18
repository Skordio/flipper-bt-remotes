#include "../bt_remotes.h"

// ---------------------------------------------------------------------------
// Validator
// ---------------------------------------------------------------------------

static bool
    cr_create_validator(const char* text, FuriString* error, void* context) {
    Hid* app = context;

    if(text[0] == '\0') {
        furi_string_set(error, "Name cannot\nbe empty");
        return false;
    }

    const char* invalid = "<>:\"/\\|?*";
    for(size_t i = 0; text[i]; i++) {
        if(strchr(invalid, text[i])) {
            furi_string_printf(error, "Char '%c'\nnot allowed", text[i]);
            return false;
        }
    }

    // Check for collision with existing remotes
    FuriString* path = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_CUSTOM_REMOTE_DIR, text, BT_REMOTES_CUSTOM_REMOTE_EXT);
    bool exists = storage_file_exists(app->storage, furi_string_get_cstr(path));
    furi_string_free(path);
    if(exists) {
        furi_string_set(error, "Name already\nin use");
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Callback
// ---------------------------------------------------------------------------

static void cr_create_text_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

// ---------------------------------------------------------------------------
// Scene handlers
// ---------------------------------------------------------------------------

void bt_remotes_scene_custom_remote_create_on_enter(void* context) {
    Hid* app = context;

    app->editing_remote.name[0] = '\0';
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Remote Name");
    text_input_set_result_callback(
        app->text_input,
        cr_create_text_cb,
        app,
        app->editing_remote.name,
        BT_REMOTES_CUSTOM_REMOTE_NAME_LEN,
        true);
    text_input_set_validator(app->text_input, cr_create_validator, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewTextInput);
}

bool bt_remotes_scene_custom_remote_create_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        // Name confirmed — clear all slots and write the file
        memset(app->editing_remote.scripts, 0, sizeof(app->editing_remote.scripts));
        bt_remotes_custom_remote_save(app);
        bt_remotes_custom_remote_load_list(app);
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneCustomRemoteEdit);
        return true;
    }
    return false;
}

void bt_remotes_scene_custom_remote_create_on_exit(void* context) {
    Hid* app = context;
    text_input_reset(app->text_input);
}
