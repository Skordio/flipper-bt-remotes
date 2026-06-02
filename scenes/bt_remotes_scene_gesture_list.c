#include "../bt_remotes.h"

// Custom Gestures library. Mirrors bt_remotes_scene_collection_list.c.
// scene state: 0=list, 1=run/manage, 2=manage menu

#define GL_IDX_CREATE 0xFE

#define GL_STATE_LIST    0
#define GL_STATE_RUNMGR  1
#define GL_STATE_MGRMENU 2

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
    submenu_add_item(app->submenu, "+ Create", GL_IDX_CREATE, gesture_list_cb, app);
}

static void build_runmgr(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->editing_gesture_name);
    submenu_add_item(app->submenu, "Run", 0, gesture_list_cb, app);
    submenu_add_item(app->submenu, "Manage", 1, gesture_list_cb, app);
}

static void build_mgrmenu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->editing_gesture_name);
    submenu_add_item(app->submenu, "Edit Lines", 0, gesture_list_cb, app);
    submenu_add_item(app->submenu, "Pin to Start", 1, gesture_list_cb, app);
    submenu_add_item(app->submenu, "Delete", 2, gesture_list_cb, app);
}

void bt_remotes_scene_gesture_list_on_enter(void* context) {
    Hid* app = context;
    bt_remotes_gesture_load_list(app);
    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneGestureList);
    if(state == GL_STATE_MGRMENU) {
        build_mgrmenu(app);
    } else if(state == GL_STATE_RUNMGR) {
        build_runmgr(app);
    } else {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneGestureList, GL_STATE_LIST);
        build_list(app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_gesture_list_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneGestureList);

    if(event.type == SceneManagerEventTypeBack) {
        if(state == GL_STATE_RUNMGR) {
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneGestureList, GL_STATE_LIST);
            build_list(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        if(state == GL_STATE_MGRMENU) {
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneGestureList, GL_STATE_RUNMGR);
            build_runmgr(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        return false; // let scene manager pop
    }

    if(event.type != SceneManagerEventTypeCustom) return false;
    uint32_t idx = event.event;

    if(state == GL_STATE_LIST) {
        if(idx == GL_IDX_CREATE) {
            app->editing_gesture_name[0]    = '\0';
            app->editing_gesture_line_count = 0;
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneGestureCreate);
            return true;
        }
        if(idx < app->gesture_count) {
            bt_remotes_gesture_load(app, app->gesture_names[idx]);
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneGestureList, GL_STATE_RUNMGR);
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
        if(idx == 1) { // Manage
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneGestureList, GL_STATE_MGRMENU);
            build_mgrmenu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
    }

    if(state == GL_STATE_MGRMENU) {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneGestureList, GL_STATE_LIST);
        if(idx == 0) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneGestureEdit);
            return true;
        }
        if(idx == 1) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneGesturePin);
            return true;
        }
        if(idx == 2) {
            bt_remotes_gesture_delete(app, app->editing_gesture_name);
            bt_remotes_gesture_load_list(app);
            build_list(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
    }

    return false;
}

void bt_remotes_scene_gesture_list_on_exit(void* context) {
    Hid* app = context;
    scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneGestureList, GL_STATE_LIST);
    submenu_reset(app->submenu);
}
