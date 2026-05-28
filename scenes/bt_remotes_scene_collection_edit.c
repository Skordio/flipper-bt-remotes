#include "../bt_remotes.h"

// scene state:
//   0        = submenu
//   1        = file browser
//   100+idx  = confirm removal of script at idx

#define CE_STATE_SUBMENU 0
#define CE_STATE_BROWSER 1
#define CE_STATE_CONFIRM_BASE 100

#define CE_IDX_ADD 0xFE

static void collection_edit_submenu_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void collection_edit_browser_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0xFF);
}

static void collection_edit_dialog_cb(DialogExResult result, void* context) {
    Hid* app = context;
    // 1 = confirmed remove, 0 = cancel
    view_dispatcher_send_custom_event(
        app->view_dispatcher, result == DialogExResultRight ? 1 : 0);
}

static void build_edit_submenu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->editing_collection_name);
    for(uint8_t i = 0; i < app->editing_collection_script_count; i++) {
        const char* base = strrchr(app->editing_collection_scripts[i], '/');
        submenu_add_item(
            app->submenu,
            base ? base + 1 : app->editing_collection_scripts[i],
            i,
            collection_edit_submenu_cb,
            app);
    }
    submenu_add_item(
        app->submenu, "+ Add Script", CE_IDX_ADD, collection_edit_submenu_cb, app);
}

void bt_remotes_scene_collection_edit_on_enter(void* context) {
    Hid* app = context;
    scene_manager_set_scene_state(
        app->scene_manager, BtRemotesSceneCollectionEdit, CE_STATE_SUBMENU);
    build_edit_submenu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_collection_edit_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneCollectionEdit);

    if(state == CE_STATE_BROWSER) {
        if(event.type == SceneManagerEventTypeCustom && event.event == 0xFF) {
            const char* path = furi_string_get_cstr(app->file_browser_result);
            if(path[0] != '\0' &&
               app->editing_collection_script_count < BT_REMOTES_COLLECTION_SCRIPT_MAX) {
                strlcpy(
                    app->editing_collection_scripts[app->editing_collection_script_count],
                    path,
                    256);
                app->editing_collection_script_count++;
                bt_remotes_collection_save(app);
            }
            file_browser_stop(app->file_browser);
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneCollectionEdit, CE_STATE_SUBMENU);
            build_edit_submenu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        if(event.type == SceneManagerEventTypeBack) {
            file_browser_stop(app->file_browser);
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneCollectionEdit, CE_STATE_SUBMENU);
            build_edit_submenu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        return false;
    }

    if(state >= CE_STATE_CONFIRM_BASE) {
        uint8_t remove_idx = (uint8_t)(state - CE_STATE_CONFIRM_BASE);
        if(event.type == SceneManagerEventTypeCustom) {
            if(event.event == 1 && remove_idx < app->editing_collection_script_count) {
                for(uint8_t j = remove_idx + 1;
                    j < app->editing_collection_script_count;
                    j++) {
                    strlcpy(
                        app->editing_collection_scripts[j - 1],
                        app->editing_collection_scripts[j],
                        256);
                }
                app->editing_collection_script_count--;
                bt_remotes_collection_save(app);
            }
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneCollectionEdit, CE_STATE_SUBMENU);
            dialog_ex_reset(app->dialog);
            build_edit_submenu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        if(event.type == SceneManagerEventTypeBack) {
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneCollectionEdit, CE_STATE_SUBMENU);
            dialog_ex_reset(app->dialog);
            build_edit_submenu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        return false;
    }

    // CE_STATE_SUBMENU
    if(event.type == SceneManagerEventTypeBack) {
        // Always return to CollectionList, even if we were pushed from CollectionCreate
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, BtRemotesSceneCollectionList);
        return true;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == CE_IDX_ADD) {
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneCollectionEdit, CE_STATE_BROWSER);
            file_browser_configure(
                app->file_browser, ".txt", DUCKY_SCRIPT_DIR, true, true, NULL, true);
            file_browser_set_callback(app->file_browser, collection_edit_browser_cb, app);
            furi_string_set(app->file_browser_result, DUCKY_SCRIPT_DIR);
            file_browser_start(app->file_browser, app->file_browser_result);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewFileBrowser);
            return true;
        }
        uint32_t idx = event.event;
        if(idx < app->editing_collection_script_count) {
            scene_manager_set_scene_state(
                app->scene_manager,
                BtRemotesSceneCollectionEdit,
                CE_STATE_CONFIRM_BASE + idx);
            dialog_ex_reset(app->dialog);
            dialog_ex_set_result_callback(app->dialog, collection_edit_dialog_cb);
            dialog_ex_set_context(app->dialog, app);
            dialog_ex_set_header(app->dialog, "Remove Script?", 64, 8, AlignCenter, AlignTop);
            const char* base = strrchr(app->editing_collection_scripts[idx], '/');
            dialog_ex_set_text(
                app->dialog,
                base ? base + 1 : app->editing_collection_scripts[idx],
                64, 28, AlignCenter, AlignTop);
            dialog_ex_set_left_button_text(app->dialog, "Cancel");
            dialog_ex_set_right_button_text(app->dialog, "Remove");
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewDialog);
            return true;
        }
    }

    return false;
}

void bt_remotes_scene_collection_edit_on_exit(void* context) {
    Hid* app = context;
    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneCollectionEdit);
    if(state == CE_STATE_BROWSER) {
        file_browser_stop(app->file_browser);
    }
    dialog_ex_reset(app->dialog);
    submenu_reset(app->submenu);
    scene_manager_set_scene_state(
        app->scene_manager, BtRemotesSceneCollectionEdit, CE_STATE_SUBMENU);
}
