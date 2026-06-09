#include "../bt_remotes.h"
#include "../helpers/gesture_runner.h"

// Runs the gesture whose file path is in app->pending_script_path (reused as the
// generic "thing to run" path). Near-copy of bt_remotes_scene_custom_actions_run.c
// but driving gesture_runner instead of ducky_runner.

enum GestureRunEvent {
    GestureRunEventDone,
    GestureRunEventError,
    GestureRunEventPopupTimeout,
};

static void gesture_run_cb(void* context) {
    Hid* app = context;
    GestureRunnerState state = gesture_runner_get_state(app->gesture_runner);
    if(state == GestureRunnerStateDone) {
        view_dispatcher_send_custom_event(app->view_dispatcher, GestureRunEventDone);
    } else if(state == GestureRunnerStateError) {
        view_dispatcher_send_custom_event(app->view_dispatcher, GestureRunEventError);
    }
}

static void gesture_run_popup_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, GestureRunEventPopupTimeout);
}

void bt_remotes_scene_gesture_run_on_enter(void* context) {
    Hid* app = context;
    gesture_runner_set_callback(app->gesture_runner, gesture_run_cb, app);
    gesture_runner_start(app->gesture_runner, app->ble_hid_profile, app->pending_script_path);
    bt_remotes_show_running_popup(app);
}

bool bt_remotes_scene_gesture_run_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        gesture_runner_stop(app->gesture_runner);
        return false;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        if(event.event == GestureRunEventDone) {
            popup_reset(app->popup);
            popup_set_header(app->popup, "Done!", 64, 22, AlignCenter, AlignCenter);
            popup_set_timeout(app->popup, 200);
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, gesture_run_popup_cb);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);
        } else if(event.event == GestureRunEventError) {
            popup_reset(app->popup);
            popup_set_header(app->popup, "Error", 64, 3, AlignCenter, AlignTop);
            popup_set_text(
                app->popup,
                gesture_runner_get_error(app->gesture_runner),
                64, 22, AlignCenter, AlignTop);
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, NULL);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);
        } else if(event.event == GestureRunEventPopupTimeout) {
            scene_manager_previous_scene(app->scene_manager);
        }
    }

    return consumed;
}

void bt_remotes_scene_gesture_run_on_exit(void* context) {
    Hid* app = context;
    gesture_runner_stop(app->gesture_runner);
    popup_reset(app->popup);
}
