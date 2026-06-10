#include "../bt_remotes.h"

#define COLLECTION_LIST_IDX_CREATE 0xFE

// scene state: 0=collection list, 1=run/manage, 2=edit/pin/delete
#define CL_STATE_LIST    0
#define CL_STATE_RUNMGR  1
#define CL_STATE_MGRMENU 2

// scene_state layout: low 8 bits = state machine value above; upper 8 bits =
// the cursor index on the LIST view (which collection the user was exploring),
// preserved through any RUNMGR/MGRMENU/sub-scene detour.
#define CL_STATE_MASK    0xFF
#define CL_CURSOR_SHIFT  8
#define CL_GET_STATE(p)  ((p) & CL_STATE_MASK)
#define CL_GET_CURSOR(p) (((p) >> CL_CURSOR_SHIFT) & 0xFF)
#define CL_PACK(state, cursor) (((uint32_t)(cursor) << CL_CURSOR_SHIFT) | ((state) & CL_STATE_MASK))

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
    uint32_t packed =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneCollectionList);
    uint8_t state = CL_GET_STATE(packed);
    uint8_t cursor = CL_GET_CURSOR(packed);
    if(state == CL_STATE_MGRMENU) {
        build_mgrmenu(app);
    } else if(state == CL_STATE_RUNMGR) {
        build_runmgr(app);
    } else {
        scene_manager_set_scene_state(
            app->scene_manager,
            BtRemotesSceneCollectionList,
            CL_PACK(CL_STATE_LIST, cursor));
        build_list(app);
        submenu_set_selected_item(app->submenu, cursor);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_collection_list_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    uint32_t packed =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneCollectionList);
    uint8_t state = CL_GET_STATE(packed);
    uint8_t list_cursor = CL_GET_CURSOR(packed);

    if(event.type == SceneManagerEventTypeBack) {
        if(state == CL_STATE_RUNMGR) {
            scene_manager_set_scene_state(
                app->scene_manager,
                BtRemotesSceneCollectionList,
                CL_PACK(CL_STATE_LIST, list_cursor));
            build_list(app);
            submenu_set_selected_item(app->submenu, list_cursor);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
        if(state == CL_STATE_MGRMENU) {
            scene_manager_set_scene_state(
                app->scene_manager,
                BtRemotesSceneCollectionList,
                CL_PACK(CL_STATE_RUNMGR, list_cursor));
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
            // Save the current LIST cursor so it's preserved across the Create flow.
            scene_manager_set_scene_state(
                app->scene_manager,
                BtRemotesSceneCollectionList,
                CL_PACK(CL_STATE_LIST, submenu_get_selected_item(app->submenu)));
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCollectionCreate);
            return true;
        }
        if(idx < app->collection_count) {
            bt_remotes_collection_load(app, app->collection_names[idx]);
            // Capture the LIST cursor on the chosen collection - it stays in the
            // upper bits through RUNMGR / MGRMENU / sub-scenes and is restored on
            // return to LIST.
            scene_manager_set_scene_state(
                app->scene_manager,
                BtRemotesSceneCollectionList,
                CL_PACK(CL_STATE_RUNMGR, (uint8_t)idx));
            build_runmgr(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
    }

    if(state == CL_STATE_RUNMGR) {
        if(idx == 0) { // Run
            scene_manager_set_scene_state(
                app->scene_manager,
                BtRemotesSceneCollectionList,
                CL_PACK(CL_STATE_LIST, list_cursor));
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneCollectionView);
            return true;
        }
        if(idx == 1) { // Manage
            scene_manager_set_scene_state(
                app->scene_manager,
                BtRemotesSceneCollectionList,
                CL_PACK(CL_STATE_MGRMENU, list_cursor));
            build_mgrmenu(app);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            return true;
        }
    }

    if(state == CL_STATE_MGRMENU) {
        // Transition back to LIST for return from any sub-scene; LIST cursor
        // preserved so the user lands on the collection they were managing.
        scene_manager_set_scene_state(
            app->scene_manager,
            BtRemotesSceneCollectionList,
            CL_PACK(CL_STATE_LIST, list_cursor));
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
    // Force state back to LIST for re-entry, but keep whatever LIST cursor was
    // already saved by on_event during the navigation that triggered this exit.
    uint32_t packed =
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneCollectionList);
    scene_manager_set_scene_state(
        app->scene_manager,
        BtRemotesSceneCollectionList,
        CL_PACK(CL_STATE_LIST, CL_GET_CURSOR(packed)));
    submenu_reset(app->submenu);
}
