#include "../bt_remotes.h"
#include "../views.h"

void bt_remotes_scene_main_on_enter(void* context) {
    Hid* app = context;

    // Check whether this remote requires a different iOS keyboard suppression mode.
    // Settings (index 14) never reaches Main; skip invalid indices.
    uint8_t idx = app->current_remote_idx;
    if(idx < BtRemotesStartIndexSettings) {
        bool want_suppress = (app->remote_kb_suppress >> idx) & 1u;
        if(want_suppress != app->ble_hid_cfg.phone_kb_suppress) {
            bt_remotes_stop_ble(app);
            app->ble_hid_cfg.phone_kb_suppress = want_suppress;
            bt_remotes_start_ble(app);
        }
    }

    view_dispatcher_switch_to_view(
        app->view_dispatcher,
        scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneMain));
}

bool bt_remotes_scene_main_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;
    UNUSED(app);
    UNUSED(event);
    return consumed;
}

void bt_remotes_scene_main_on_exit(void* context) {
    Hid* app = context;
    UNUSED(app);
}
