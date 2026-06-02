#include "../bt_remotes.h"

static void gesture_create_text_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

static bool gesture_create_validator(const char* text, FuriString* error, void* context) {
    Hid* app = context;

    if(!bt_remotes_validate_name(text, error)) return false;

    for(uint8_t i = 0; i < app->gesture_count; i++) {
        if(strcmp(app->gesture_names[i], text) == 0) {
            furi_string_set(error, "Name already\nin use");
            return false;
        }
    }
    return true;
}

void bt_remotes_scene_gesture_create_on_enter(void* context) {
    Hid* app = context;

    app->editing_gesture_name[0]    = '\0';
    app->editing_gesture_line_count = 0;

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Gesture Name");
    text_input_set_result_callback(
        app->text_input,
        gesture_create_text_cb,
        app,
        app->editing_gesture_name,
        BT_REMOTES_GESTURE_NAME_LEN,
        true);
    text_input_set_validator(app->text_input, gesture_create_validator, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewTextInput);
}

bool bt_remotes_scene_gesture_create_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        // Name confirmed — create empty gesture, reload list, push the line editor.
        bt_remotes_gesture_save(app);
        bt_remotes_gesture_load_list(app);
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneGestureEdit);
        return true;
    }
    return false;
}

void bt_remotes_scene_gesture_create_on_exit(void* context) {
    Hid* app = context;
    text_input_reset(app->text_input);
}
