#include "../bt_remotes.h"
#include "../helpers/ducky_runner.h"

#define CA_STATE_SUBMENU 0
#define CA_STATE_BROWSER 1

// scene_state layout: low 8 bits = state machine (SUBMENU/BROWSER),
// upper bits = saved submenu cursor (0..1, but we use 8 bits for headroom).
#define CA_STATE_MASK     0xFF
#define CA_CURSOR_SHIFT   8
#define CA_GET_STATE(p)   ((p) & CA_STATE_MASK)
#define CA_GET_CURSOR(p)  (((p) >> CA_CURSOR_SHIFT) & 0xFF)
#define CA_PACK(state, cursor) (((uint32_t)(cursor) << CA_CURSOR_SHIFT) | ((state) & CA_STATE_MASK))

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
    // Caller is responsible for setting the cursor (so we don't clobber a
    // restored cursor here).
}

void bt_remotes_scene_custom_actions_on_enter(void* context) {
    Hid* app = context;

    // "Connect per run": stay disconnected while browsing scripts; the run scene
    // brings BLE up only for a script's execution. Idempotent; also re-runs when
    // returning here from the file browser / a finished run.
    bt_remotes_ducky_browse_enter(app);

    // Preserve the saved cursor while resetting state to SUBMENU (we always
    // re-enter the hub, never the file browser).
    uint32_t saved = scene_manager_get_scene_state(
        app->scene_manager, BtRemotesSceneCustomActions);
    uint8_t cursor = CA_GET_CURSOR(saved);
    scene_manager_set_scene_state(
        app->scene_manager, BtRemotesSceneCustomActions, CA_PACK(CA_STATE_SUBMENU, cursor));

    build_hub_submenu(app);
    submenu_set_selected_item(app->submenu, cursor);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_custom_actions_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    uint32_t packed =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneCustomActions);
    uint8_t state = CA_GET_STATE(packed);
    uint8_t cursor = CA_GET_CURSOR(packed);

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
                app->scene_manager,
                BtRemotesSceneCustomActions,
                CA_PACK(CA_STATE_SUBMENU, cursor));
            build_hub_submenu(app);
            submenu_set_selected_item(app->submenu, cursor);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        return false;
    }

    // CA_STATE_SUBMENU
    if(event.type == SceneManagerEventTypeCustom) {
        // The user just clicked a row; the submenu cursor is now on that row.
        uint8_t live_cursor = submenu_get_selected_item(app->submenu);
        if(event.event == CustomActionsIdxBrowse) {
            scene_manager_set_scene_state(
                app->scene_manager,
                BtRemotesSceneCustomActions,
                CA_PACK(CA_STATE_BROWSER, live_cursor));
            file_browser_configure(
                app->file_browser, ".txt", DUCKY_SCRIPT_DIR, true, true, NULL, true);
            file_browser_set_callback(app->file_browser, bt_remotes_file_browser_cb, app);
            furi_string_set(app->file_browser_result, DUCKY_SCRIPT_DIR);
            file_browser_start(app->file_browser, app->file_browser_result);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewFileBrowser);
            return true;
        } else if(event.event == CustomActionsIdxCollections) {
            scene_manager_set_scene_state(
                app->scene_manager,
                BtRemotesSceneCustomActions,
                CA_PACK(CA_STATE_SUBMENU, live_cursor));
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCollectionList);
            return true;
        }
    }

    return false;
}

void bt_remotes_scene_custom_actions_on_exit(void* context) {
    Hid* app = context;
    uint32_t packed =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneCustomActions);
    uint8_t state = CA_GET_STATE(packed);
    uint8_t cursor = CA_GET_CURSOR(packed);
    // If we're in the submenu state, the live cursor is more up-to-date than the
    // packed one (e.g. the user scrolled but didn't click before backing out).
    if(state == CA_STATE_SUBMENU) {
        cursor = submenu_get_selected_item(app->submenu);
    } else if(state == CA_STATE_BROWSER) {
        file_browser_stop(app->file_browser);
    }
    scene_manager_set_scene_state(
        app->scene_manager,
        BtRemotesSceneCustomActions,
        CA_PACK(CA_STATE_SUBMENU, cursor));
    submenu_reset(app->submenu);
}
