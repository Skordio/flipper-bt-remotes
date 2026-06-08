#include "../bt_remotes.h"
#include "../helpers/ducky_runner.h"

#define CA_STATE_SUBMENU 0
#define CA_STATE_BROWSER 1

enum CustomActionsSubmenuIdx {
    CustomActionsIdxBrowse = 0,
    CustomActionsIdxCollections = 1,
};

static void bt_remotes_custom_actions_submenu_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void bt_remotes_file_browser_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 1);
}

static void build_hub_submenu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Ducky Scripts");
    submenu_add_item(
        app->submenu,
        "Browse Files",
        CustomActionsIdxBrowse,
        bt_remotes_custom_actions_submenu_cb,
        app);
    submenu_add_item(
        app->submenu,
        "Collections",
        CustomActionsIdxCollections,
        bt_remotes_custom_actions_submenu_cb,
        app);
    submenu_set_selected_item(app->submenu, 0);
}

void bt_remotes_scene_custom_actions_on_enter(void* context) {
    Hid* app = context;

    // "Connect per run": stay disconnected while browsing scripts; the run scene
    // brings BLE up only for a script's execution. Idempotent; also re-runs when
    // returning here from the file browser / a finished run.
    if(app->ducky_connect_per_run) bt_remotes_stop_ble(app);

    scene_manager_set_scene_state(
        app->scene_manager, BtRemotesSceneCustomActions, CA_STATE_SUBMENU);

    build_hub_submenu(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_custom_actions_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneCustomActions);

    if(state == CA_STATE_BROWSER) {
        if(event.type == SceneManagerEventTypeCustom && event.event == 1) {
            strlcpy(
                app->pending_script_path,
                furi_string_get_cstr(app->file_browser_result),
                sizeof(app->pending_script_path));
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCustomActionsRun);
            return true;
        }
        if(event.type == SceneManagerEventTypeBack) {
            // Return to hub submenu instead of popping scene
            file_browser_stop(app->file_browser);
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneCustomActions, CA_STATE_SUBMENU);
            build_hub_submenu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        return false;
    }

    // CA_STATE_SUBMENU
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == CustomActionsIdxBrowse) {
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneCustomActions, CA_STATE_BROWSER);
            file_browser_configure(
                app->file_browser, ".txt", DUCKY_SCRIPT_DIR, true, true, NULL, true);
            file_browser_set_callback(app->file_browser, bt_remotes_file_browser_cb, app);
            furi_string_set(app->file_browser_result, DUCKY_SCRIPT_DIR);
            file_browser_start(app->file_browser, app->file_browser_result);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewFileBrowser);
            return true;
        } else if(event.event == CustomActionsIdxCollections) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCollectionList);
            return true;
        }
    }

    return false;
}

void bt_remotes_scene_custom_actions_on_exit(void* context) {
    Hid* app = context;
    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneCustomActions);
    if(state == CA_STATE_BROWSER) {
        file_browser_stop(app->file_browser);
    }
    submenu_reset(app->submenu);
}
