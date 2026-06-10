#include "../bt_remotes.h"

// Custom Gestures read-only browser. Gestures are created/edited on PC.
// Selecting a gesture shows Run and Pin to Start options.

#define GL_STATE_LIST   0
#define GL_STATE_RUNMGR 1

// scene_state layout: low 8 bits = state machine value above; upper 8 bits =
// the cursor index on the LIST view (which gesture the user was exploring).
#define GL_STATE_MASK    0xFF
#define GL_CURSOR_SHIFT  8
#define GL_GET_STATE(p)  ((p) & GL_STATE_MASK)
#define GL_GET_CURSOR(p) (((p) >> GL_CURSOR_SHIFT) & 0xFF)
#define GL_PACK(state, cursor) (((uint32_t)(cursor) << GL_CURSOR_SHIFT) | ((state) & GL_STATE_MASK))

static void gesture_list_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void build_list(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Custom Gestures");
    for(uint8_t i = 0; i < app->gesture_count; i++) {
        submenu_add_item(app->submenu, app->gesture_names[i], i, gesture_list_cb, app);
    }
    if(app->gesture_count == 0) {
        submenu_add_item(app->submenu, "(no gestures on SD card)", 0xFF, NULL, NULL);
    }
}

static void build_runmgr(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->editing_gesture_name);
    submenu_add_item(app->submenu, "Run", 0, gesture_list_cb, app);
    submenu_add_item(app->submenu, "Pin to Start", 1, gesture_list_cb, app);
}

void bt_remotes_scene_gesture_list_on_enter(void* context) {
    Hid* app = context;
    bt_remotes_gesture_load_list(app);
    uint32_t packed =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneGestureList);
    uint8_t state = GL_GET_STATE(packed);
    uint8_t cursor = GL_GET_CURSOR(packed);
    if(state == GL_STATE_RUNMGR) {
        build_runmgr(app);
    } else {
        scene_manager_set_scene_state(
            app->scene_manager,
            BtRemotesSceneGestureList,
            GL_PACK(GL_STATE_LIST, cursor));
        build_list(app);
        submenu_set_selected_item(app->submenu, cursor);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_gesture_list_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    uint32_t packed =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneGestureList);
    uint8_t state = GL_GET_STATE(packed);
    uint8_t list_cursor = GL_GET_CURSOR(packed);

    if(event.type == SceneManagerEventTypeBack) {
        if(state == GL_STATE_RUNMGR) {
            scene_manager_set_scene_state(
                app->scene_manager,
                BtRemotesSceneGestureList,
                GL_PACK(GL_STATE_LIST, list_cursor));
            build_list(app);
            submenu_set_selected_item(app->submenu, list_cursor);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        return false; // let scene manager pop
    }

    if(event.type != SceneManagerEventTypeCustom) return false;
    uint32_t idx = event.event;

    if(state == GL_STATE_LIST) {
        if(idx < app->gesture_count) {
            strlcpy(app->editing_gesture_name, app->gesture_names[idx],
                    sizeof(app->editing_gesture_name));
            // Save the LIST cursor on the chosen gesture so any sub-scene return
            // (Run / Pin) restores it.
            scene_manager_set_scene_state(
                app->scene_manager,
                BtRemotesSceneGestureList,
                GL_PACK(GL_STATE_RUNMGR, (uint8_t)idx));
            build_runmgr(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
    }

    if(state == GL_STATE_RUNMGR) {
        if(idx == 0) { // Run
            bt_remotes_gesture_path(
                app->editing_gesture_name,
                app->pending_script_path,
                sizeof(app->pending_script_path));
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneGestureRun);
            return true;
        }
        if(idx == 1) { // Pin to Start
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneGesturePin);
            return true;
        }
    }

    return false;
}

void bt_remotes_scene_gesture_list_on_exit(void* context) {
    Hid* app = context;
    // Reset state machine to LIST for re-entry, but preserve the LIST cursor
    // already saved by on_event when navigating to a sub-scene.
    uint32_t packed =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneGestureList);
    scene_manager_set_scene_state(
        app->scene_manager,
        BtRemotesSceneGestureList,
        GL_PACK(GL_STATE_LIST, GL_GET_CURSOR(packed)));
    submenu_reset(app->submenu);
}
