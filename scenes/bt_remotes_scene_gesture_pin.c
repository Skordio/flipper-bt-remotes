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
        // Unpin
        for(uint8_t j = pin_idx; j + 1 < app->pinned_count; j++) {
            strlcpy(
                app->pinned_collections[j],
                app->pinned_collections[j + 1],
                BT_REMOTES_COLLECTION_NAME_LEN);
            app->pinned_kinds[j] = app->pinned_kinds[j + 1];
        }
        app->pinned_count--;
        bt_remotes_pinned_save(app);
        popup_set_header(app->popup, "Unpinned!", 64, 30, AlignCenter, AlignCenter);
    } else {
        if(app->pinned_count < BT_REMOTES_PINNED_MAX) {
            strlcpy(
                app->pinned_collections[app->pinned_count],
                app->editing_gesture_name,
                BT_REMOTES_COLLECTION_NAME_LEN);
            app->pinned_kinds[app->pinned_count] = 1; // gesture
            app->pinned_count++;
            bt_remotes_pinned_save(app);
            popup_set_header(app->popup, "Pinned!", 64, 30, AlignCenter, AlignCenter);
        } else {
            popup_set_header(app->popup, "Pin list full", 64, 30, AlignCenter, AlignCenter);
        }
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
