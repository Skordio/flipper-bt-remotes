#include "../bt_remotes.h"

// Start Menu Layout sub-scene under Profile Settings. Pure chooser: Hide Remote
// Types and Reset Menu Order each open their existing dedicated scene.

enum BtRemotesProfileMenuLayoutIndex {
    BtRemotesProfileMenuLayoutIndexHide,
    BtRemotesProfileMenuLayoutIndexReset,
    BtRemotesProfileMenuLayoutIndexHelp,
};

static void bt_remotes_scene_profile_menu_layout_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bt_remotes_scene_profile_menu_layout_on_enter(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Start Menu Layout");

    submenu_add_item(
        app->submenu,
        "Hide Remote Types",
        BtRemotesProfileMenuLayoutIndexHide,
        bt_remotes_scene_profile_menu_layout_cb,
        app);
    submenu_add_item(
        app->submenu,
        "Reset Menu Order",
        BtRemotesProfileMenuLayoutIndexReset,
        bt_remotes_scene_profile_menu_layout_cb,
        app);
    submenu_add_item(
        app->submenu,
        "Help",
        BtRemotesProfileMenuLayoutIndexHelp,
        bt_remotes_scene_profile_menu_layout_cb,
        app);

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_profile_menu_layout_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    if(event.event == BtRemotesProfileMenuLayoutIndexHide) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneHideItems);
    } else if(event.event == BtRemotesProfileMenuLayoutIndexReset) {
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneResetMenu);
    } else if(event.event == BtRemotesProfileMenuLayoutIndexHelp) {
        scene_manager_set_scene_state(
            app->scene_manager, BtRemotesSceneRemoteSettingsHelp, RemoteSettingsHelpMenuLayout);
        scene_manager_next_scene(app->scene_manager, BtRemotesSceneRemoteSettingsHelp);
    }
    return true;
}

void bt_remotes_scene_profile_menu_layout_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
