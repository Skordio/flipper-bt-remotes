#include "../bt_remotes.h"

#define COLLECTION_LIST_IDX_CREATE 0xFE

// scene state: 0=collection list, 1=run/manage, 2=edit/pin/delete
#define CL_STATE_LIST    0
#define CL_STATE_RUNMGR  1
#define CL_STATE_MGRMENU 2

static void collection_list_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void build_list(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Collections");
    for(uint8_t i = 0; i < app->collection_count; i++) {
        submenu_add_item(app->submenu, app->collection_names[i], i, collection_list_cb, app);
    }
    submenu_add_item(
        app->submenu, "+ Create", COLLECTION_LIST_IDX_CREATE, collection_list_cb, app);
}

static void build_runmgr(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->editing_collection_name);
    submenu_add_item(app->submenu, "Run",    0, collection_list_cb, app);
    submenu_add_item(app->submenu, "Manage", 1, collection_list_cb, app);
}

static void build_mgrmenu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, app->editing_collection_name);
    submenu_add_item(app->submenu, "Edit Scripts", 0, collection_list_cb, app);
    submenu_add_item(app->submenu, "Pin to Start",  1, collection_list_cb, app);
    submenu_add_item(app->submenu, "Delete",         2, collection_list_cb, app);
}

void bt_remotes_scene_collection_list_on_enter(void* context) {
    Hid* app = context;
    bt_remotes_collection_load_list(app);
    scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneCollectionList, CL_STATE_LIST);
    build_list(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_collection_list_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    uint32_t state =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneCollectionList);

    if(event.type == SceneManagerEventTypeBack) {
        if(state == CL_STATE_RUNMGR) {
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneCollectionList, CL_STATE_LIST);
            build_list(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        if(state == CL_STATE_MGRMENU) {
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneCollectionList, CL_STATE_RUNMGR);
            build_runmgr(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        return false; // let scene manager pop
    }

    if(event.type != SceneManagerEventTypeCustom) return false;
    uint32_t idx = event.event;

    if(state == CL_STATE_LIST) {
        if(idx == COLLECTION_LIST_IDX_CREATE) {
            app->editing_collection_name[0] = '\0';
            app->editing_collection_script_count = 0;
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCollectionCreate);
            return true;
        }
        if(idx < app->collection_count) {
            bt_remotes_collection_load(app, app->collection_names[idx]);
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneCollectionList, CL_STATE_RUNMGR);
            build_runmgr(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
    }

    if(state == CL_STATE_RUNMGR) {
        if(idx == 0) { // Run
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneCollectionList, CL_STATE_LIST);
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCollectionView);
            return true;
        }
        if(idx == 1) { // Manage
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneCollectionList, CL_STATE_MGRMENU);
            build_mgrmenu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
    }

    if(state == CL_STATE_MGRMENU) {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneCollectionList, CL_STATE_LIST);
        if(idx == 0) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCollectionEdit);
            return true;
        }
        if(idx == 1) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCollectionPin);
            return true;
        }
        if(idx == 2) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCollectionDelete);
            return true;
        }
    }

    return false;
}

void bt_remotes_scene_collection_list_on_exit(void* context) {
    Hid* app = context;
    scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneCollectionList, CL_STATE_LIST);
    submenu_reset(app->submenu);
}
