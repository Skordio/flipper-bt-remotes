#include "../bt_remotes.h"
#include "../helpers/ducky_runner.h"

// ---------------------------------------------------------------------------
// FileBrowser callback — fired on the main thread when a file is chosen
// ---------------------------------------------------------------------------

static void bt_remotes_file_browser_cb(void* context) {
    Hid* app = context;
    // The selected path is already in app->file_browser_result; just notify
    // the scene manager so on_event can pick it up.
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

// ---------------------------------------------------------------------------
// Scene handlers
// ---------------------------------------------------------------------------

void bt_remotes_scene_custom_actions_on_enter(void* context) {
    Hid* app = context;

    file_browser_configure(
        app->file_browser,
        ".txt",           // only list .txt files
        DUCKY_SCRIPT_DIR, // root of the browser — user cannot navigate above this
        true,             // skip_assets: hide "assets" directories
        true,             // hide_dot_files
        NULL,             // file icon: use default
        true);            // hide_ext: show filenames without the .txt suffix

    file_browser_set_callback(app->file_browser, bt_remotes_file_browser_cb, app);

    // Start browsing at the root; reuse the result string as the initial path
    furi_string_set(app->file_browser_result, DUCKY_SCRIPT_DIR);
    file_browser_start(app->file_browser, app->file_browser_result);

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewFileBrowser);
}

bool bt_remotes_scene_custom_actions_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;

    if(event.type == SceneManagerEventTypeCustom) {
        // A .txt file was selected — copy the full path and run it
        strlcpy(
            app->pending_script_path,
            furi_string_get_cstr(app->file_browser_result),
            sizeof(app->pending_script_path));
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneCustomActionsRun);
        return true;
    }

    // Back at the browser root: let the scene manager pop this scene
    return false;
}

void bt_remotes_scene_custom_actions_on_exit(void* context) {
    Hid* app = context;
    file_browser_stop(app->file_browser);
}
