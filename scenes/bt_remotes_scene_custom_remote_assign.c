#include "../bt_remotes.h"
#include "../helpers/ducky_runner.h"

// Custom event codes — distinct from submenu item indices (0, 1)
#define ASSIGN_EV_BROWSE    0
#define ASSIGN_EV_UNASSIGN  1
#define ASSIGN_EV_FILE_SEL  2   // fired by the file browser callback

// Scene state: 0 = submenu showing, 1 = file browser open
#define ASSIGN_STATE_SUBMENU  0
#define ASSIGN_STATE_BROWSER  1

// ---------------------------------------------------------------------------
// File browser callback — fires on main thread when a file is picked
// ---------------------------------------------------------------------------

static void cr_assign_file_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, ASSIGN_EV_FILE_SEL);
}

// ---------------------------------------------------------------------------
// Submenu callback
// ---------------------------------------------------------------------------

static void cr_assign_submenu_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

// ---------------------------------------------------------------------------
// Scene handlers
// ---------------------------------------------------------------------------

void bt_remotes_scene_custom_remote_assign_on_enter(void* context) {
    Hid* app = context;
    scene_manager_set_scene_state(
        app->scene_manager, BtRemotesSceneCustomRemoteAssign, ASSIGN_STATE_SUBMENU);

    bool assigned = app->editing_remote.scripts[app->editing_remote_input_idx][0] != '\0';

    submenu_reset(app->submenu);
    submenu_set_header(
        app->submenu, custom_remote_input_labels[app->editing_remote_input_idx]);
    submenu_add_item(
        app->submenu, "Browse Scripts", ASSIGN_EV_BROWSE, cr_assign_submenu_cb, app);
    if(assigned) {
        submenu_add_item(
            app->submenu, "Unassign", ASSIGN_EV_UNASSIGN, cr_assign_submenu_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_custom_remote_assign_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;

    if(event.type != SceneManagerEventTypeCustom) return false;

    uint32_t state = scene_manager_get_scene_state(
        app->scene_manager, BtRemotesSceneCustomRemoteAssign);

    if(state == ASSIGN_STATE_SUBMENU) {
        if(event.event == ASSIGN_EV_BROWSE) {
            // Open the file browser
            file_browser_configure(
                app->file_browser,
                ".txt",
                DUCKY_SCRIPT_DIR,
                true,  // skip_assets
                true,  // hide_dot_files
                NULL,  // default icon
                true); // hide_ext
            file_browser_set_callback(app->file_browser, cr_assign_file_cb, app);
            furi_string_set(app->file_browser_result, DUCKY_SCRIPT_DIR);
            file_browser_start(app->file_browser, app->file_browser_result);
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneCustomRemoteAssign, ASSIGN_STATE_BROWSER);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewFileBrowser);
            return true;

        } else if(event.event == ASSIGN_EV_UNASSIGN) {
            // Clear the slot and return to the edit scene
            app->editing_remote.scripts[app->editing_remote_input_idx][0] = '\0';
            bt_remotes_custom_remote_save(app);
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }

    } else if(state == ASSIGN_STATE_BROWSER) {
        if(event.event == ASSIGN_EV_FILE_SEL) {
            // File selected — save it to the slot
            strlcpy(
                app->editing_remote.scripts[app->editing_remote_input_idx],
                furi_string_get_cstr(app->file_browser_result),
                BT_REMOTES_CUSTOM_REMOTE_SCRIPT_LEN);
            bt_remotes_custom_remote_save(app);
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }

    return false;
}

void bt_remotes_scene_custom_remote_assign_on_exit(void* context) {
    Hid* app = context;
    uint32_t state = scene_manager_get_scene_state(
        app->scene_manager, BtRemotesSceneCustomRemoteAssign);
    if(state == ASSIGN_STATE_BROWSER) {
        file_browser_stop(app->file_browser);
    }
    submenu_reset(app->submenu);
}
