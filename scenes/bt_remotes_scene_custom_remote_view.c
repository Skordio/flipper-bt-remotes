#include "../bt_remotes.h"
#include "../views/hid_custom_remote.h"

// ---------------------------------------------------------------------------
// View callback — fired when the user presses a mapped button
// ---------------------------------------------------------------------------

static void cr_view_input_cb(void* context, CustomRemoteInputSlot slot) {
    Hid* app = context;
    // Only act if a script is assigned to this slot
    if(app->editing_remote.scripts[slot][0] == '\0') return;

    strlcpy(
        app->pending_script_path,
        app->editing_remote.scripts[slot],
        sizeof(app->pending_script_path));
    // Use slot as the custom event value; scene converts it to a script run
    view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)slot);
}

// ---------------------------------------------------------------------------
// Scene handlers
// ---------------------------------------------------------------------------

void bt_remotes_scene_custom_remote_view_on_enter(void* context) {
    Hid* app = context;
    // Reload the remote from disk in case scripts were changed since last entry
    bt_remotes_custom_remote_load(app, app->editing_remote.name);
    hid_custom_remote_set_remote(app->hid_custom_remote, &app->editing_remote);
    hid_custom_remote_set_callback(app->hid_custom_remote, cr_view_input_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewCustomRemote);
}

bool bt_remotes_scene_custom_remote_view_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        // A script was triggered — push the existing Ducky script runner scene.
        // When done it calls scene_manager_previous_scene(), re-entering on_enter here.
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneCustomActionsRun);
        return true;
    }
    // Back events: let the scene manager pop this scene (returns to Start menu)
    return false;
}

void bt_remotes_scene_custom_remote_view_on_exit(void* context) {
    UNUSED(context);
}
