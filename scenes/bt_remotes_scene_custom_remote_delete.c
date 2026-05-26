#include "../bt_remotes.h"

// Event codes — submenu fires with item index (0..N-1); dialog uses high sentinel codes.
#define DELETE_EV_CONFIRM 0xFE
#define DELETE_EV_CANCEL  0xFF

static void cr_delete_submenu_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void cr_delete_dialog_cb(DialogExResult result, void* context) {
    Hid* app = context;
    if(result == DialogExResultRight) {
        view_dispatcher_send_custom_event(app->view_dispatcher, DELETE_EV_CONFIRM);
    } else {
        view_dispatcher_send_custom_event(app->view_dispatcher, DELETE_EV_CANCEL);
    }
}

static void build_delete_submenu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Delete Remote");

    if(app->custom_remote_count == 0) {
        submenu_add_item(app->submenu, "(No remotes)", 0xFF, NULL, NULL);
    } else {
        for(uint8_t i = 0; i < app->custom_remote_count; i++) {
            submenu_add_item(
                app->submenu, app->custom_remote_names[i], i, cr_delete_submenu_cb, app);
        }
    }
}

void bt_remotes_scene_custom_remote_delete_on_enter(void* context) {
    Hid* app = context;
    bt_remotes_custom_remote_load_list(app);
    build_delete_submenu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_custom_remote_delete_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event < app->custom_remote_count) {
        // Submenu: item selected — store name and show confirm dialog
        strlcpy(app->pending_name, app->custom_remote_names[event.event],
                sizeof(app->pending_name));

        dialog_ex_reset(app->dialog);
        dialog_ex_set_header(app->dialog, "Delete Remote?", 64, 3, AlignCenter, AlignTop);
        dialog_ex_set_text(app->dialog, app->pending_name, 64, 22, AlignCenter, AlignTop);
        dialog_ex_set_left_button_text(app->dialog, "Cancel");
        dialog_ex_set_right_button_text(app->dialog, "Delete");
        dialog_ex_set_result_callback(app->dialog, cr_delete_dialog_cb);
        dialog_ex_set_context(app->dialog, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, HidViewDialog);
        return true;

    } else if(event.event == DELETE_EV_CONFIRM) {
        // Confirmed — delete the remote
        bt_remotes_custom_remote_delete(app, app->pending_name);
        bt_remotes_custom_remote_load_list(app);

        if(app->custom_remote_count == 0) {
            // No more remotes — pop back to hub
            scene_manager_previous_scene(app->scene_manager);
        } else {
            // Rebuild and show the list again
            build_delete_submenu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
        }
        return true;

    } else if(event.event == DELETE_EV_CANCEL) {
        // Cancelled — back to the submenu
        view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
        return true;
    }

    return false;
}

void bt_remotes_scene_custom_remote_delete_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
    dialog_ex_reset(app->dialog);
}
