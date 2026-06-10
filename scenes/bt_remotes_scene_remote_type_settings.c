#include "../bt_remotes.h"

enum BtRemotesRemoteTypeIndex {
    BtRemotesRemoteTypeIndexKeynote = 0,
    BtRemotesRemoteTypeIndexMedia   = 1,
    BtRemotesRemoteTypeIndexTikTok  = 2,
    BtRemotesRemoteTypeIndexDucky   = 3,
    BtRemotesRemoteTypeIndexHelp    = 4,
};

static void bt_remotes_scene_remote_type_settings_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void bt_remotes_scene_remote_type_settings_on_enter(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Per-Remote Settings");
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
    submenu_add_item(
        app->submenu,
        "TikTok / YT Shorts",
        BtRemotesRemoteTypeIndexTikTok,
        bt_remotes_scene_remote_type_settings_cb,
        app);
    submenu_add_item(
        app->submenu,
        "DuckyScript",
        BtRemotesRemoteTypeIndexDucky,
        bt_remotes_scene_remote_type_settings_cb,
        app);
    submenu_add_item(
        app->submenu,
        "Help",
        BtRemotesRemoteTypeIndexHelp,
        bt_remotes_scene_remote_type_settings_cb,
        app);
    submenu_set_selected_item(
        app->submenu,
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneRemoteTypeSettings));
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
        } else if(event.event == BtRemotesRemoteTypeIndexTikTok) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneTikTokSettings);
            consumed = true;
        } else if(event.event == BtRemotesRemoteTypeIndexDucky) {
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneDuckySettings);
            consumed = true;
        } else if(event.event == BtRemotesRemoteTypeIndexHelp) {
            scene_manager_set_scene_state(
                app->scene_manager,
                BtRemotesSceneRemoteSettingsHelp,
                RemoteSettingsHelpPerRemote);
            scene_manager_next_scene(app->scene_manager, BtRemotesSceneRemoteSettingsHelp);
            consumed = true;
        }
    }
    return consumed;
}

void bt_remotes_scene_remote_type_settings_on_exit(void* context) {
    Hid* app = context;
    scene_manager_set_scene_state(
        app->scene_manager,
        BtRemotesSceneRemoteTypeSettings,
        submenu_get_selected_item(app->submenu));
    submenu_reset(app->submenu);
}
