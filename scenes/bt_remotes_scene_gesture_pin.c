#include "../bt_remotes.h"

// Pin / unpin the active gesture (app->editing_gesture_name) to the Start menu.
// Gesture pins share the per-profile pin list but are tagged kind=1 so the Start
// scene routes them to the gesture runner instead of the collection viewer.

static void gesture_pin_popup_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void bt_remotes_scene_gesture_pin_on_enter(void* context) {
    Hid* app = context;

    bool    already_pinned = false;
    uint8_t pin_idx        = 0;
    for(uint8_t i = 0; i < app->pinned_count; i++) {
        if(app->pinned_kinds[i] == 1 &&
           strcmp(app->pinned_collections[i], app->editing_gesture_name) == 0) {
            already_pinned = true;
            pin_idx        = i;
            break;
        }
    }

    if(already_pinned) {
        bt_remotes_pin_remove(app, pin_idx);
        popup_set_header(app->popup, "Unpinned!", 64, 30, AlignCenter, AlignCenter);
    } else if(bt_remotes_pin_add(app, app->editing_gesture_name, 1)) {
        popup_set_header(app->popup, "Pinned!", 64, 30, AlignCenter, AlignCenter);
    } else {
        popup_set_header(app->popup, "Pin list full", 64, 30, AlignCenter, AlignCenter);
    }

    popup_set_timeout(app->popup, 1000);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, gesture_pin_popup_cb);
    popup_enable_timeout(app->popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);
}

bool bt_remotes_scene_gesture_pin_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    if(event.type == SceneManagerEventTypeCustom && event.event == 0) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void bt_remotes_scene_gesture_pin_on_exit(void* context) {
    Hid* app = context;
    popup_reset(app->popup);
}
