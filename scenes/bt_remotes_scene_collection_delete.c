#include "../bt_remotes.h"

static void collection_delete_dialog_cb(DialogExResult result, void* context) {
    Hid* app = context;
    // 1 = confirmed delete, 0 = cancel
    view_dispatcher_send_custom_event(
        app->view_dispatcher, result == DialogExResultRight ? 1 : 0);
}

void bt_remotes_scene_collection_delete_on_enter(void* context) {
    Hid* app = context;

    dialog_ex_reset(app->dialog);
    dialog_ex_set_result_callback(app->dialog, collection_delete_dialog_cb);
    dialog_ex_set_context(app->dialog, app);
    dialog_ex_set_header(app->dialog, "Delete Collection?", 64, 3, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog, app->editing_collection_name, 64, 22, AlignCenter, AlignTop);
    dialog_ex_set_left_button_text(app->dialog, "Cancel");
    dialog_ex_set_right_button_text(app->dialog, "Delete");

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewDialog);
}

bool bt_remotes_scene_collection_delete_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == 1) {
            bt_remotes_collection_delete(app, app->editing_collection_name);
            bt_remotes_collection_load_list(app);
            scene_manager_search_and_switch_to_previous_scene(
                app->scene_manager, BtRemotesSceneCollectionList);
        } else {
            // Cancel — restore manage submenu level in CollectionList before popping
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneCollectionList, 2); // CL_STATE_MGRMENU
            scene_manager_previous_scene(app->scene_manager);
        }
        return true;
    }

    return false;
}

void bt_remotes_scene_collection_delete_on_exit(void* context) {
    Hid* app = context;
    dialog_ex_reset(app->dialog);
}
