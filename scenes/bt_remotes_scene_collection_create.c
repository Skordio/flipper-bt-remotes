#include "../bt_remotes.h"

static void collection_create_text_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

static bool
    collection_create_validator(const char* text, FuriString* error, void* context) {
    Hid* app = context;

    if(!bt_remotes_validate_name(text, error)) return false;

    for(uint8_t i = 0; i < app->collection_count; i++) {
        if(strcmp(app->collection_names[i], text) == 0) {
            furi_string_set(error, "Name already\nin use");
            return false;
        }
    }

    return true;
}

void bt_remotes_scene_collection_create_on_enter(void* context) {
    Hid* app = context;

    app->editing_collection_name[0] = '\0';
    app->editing_collection_script_count = 0;

    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Collection Name");
    text_input_set_result_callback(
        app->text_input,
        collection_create_text_cb,
        app,
        app->editing_collection_name,
        BT_REMOTES_COLLECTION_NAME_LEN,
        true);
    text_input_set_validator(app->text_input, collection_create_validator, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewTextInput);
}

bool bt_remotes_scene_collection_create_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;

    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        // Name confirmed — create empty collection, reload list, push edit scene
        bt_remotes_collection_save(app);
        bt_remotes_collection_load_list(app);
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneCollectionEdit);
        return true;
    }

    return false;
}

void bt_remotes_scene_collection_create_on_exit(void* context) {
    Hid* app = context;
    text_input_reset(app->text_input);
}
