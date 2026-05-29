#include "../bt_remotes.h"

enum BtRemotesRemoteTypeIndex {
    BtRemotesRemoteTypeIndexKeynote = 0,
    BtRemotesRemoteTypeIndexMedia   = 1,
};

static void bt_remotes_scene_remote_type_settings_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bt_remotes_scene_remote_type_settings_on_enter(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Remote Type Settings");
    submenu_add_item(
        app->submenu,
        "Keynote",
        BtRemotesRemoteTypeIndexKeynote,
        bt_remotes_scene_remote_type_settings_cb,
        app);
    submenu_add_item(
        app->submenu,
        "Media",
        BtRemotesRemoteTypeIndexMedia,
        bt_remotes_scene_remote_type_settings_cb,
        app);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_remote_type_settings_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtRemotesRemoteTypeIndexKeynote) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneKeynoteSettings);
            consumed = true;
        } else if(event.event == BtRemotesRemoteTypeIndexMedia) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneMediaSettings);
            consumed = true;
        }
    }
    return consumed;
}

void bt_remotes_scene_remote_type_settings_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
}
