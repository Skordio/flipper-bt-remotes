#include "../bt_remotes.h"
#include "../helpers/ducky_runner.h"

enum BtRemotesCustomActionsRunEvent {
    CustomActionsRunEventDone,
    CustomActionsRunEventError,
    CustomActionsRunEventPopupTimeout,
};

// ---------------------------------------------------------------------------
// Runner callback — called from the DuckyRunner worker thread
// ---------------------------------------------------------------------------

static void custom_actions_run_cb(void* context) {
    Hid* app = context;
    DuckyRunnerState state = ducky_runner_get_state(app->ducky_runner);
    if(state == DuckyRunnerStateDone) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CustomActionsRunEventDone);
    } else if(state == DuckyRunnerStateError) {
        view_dispatcher_send_custom_event(app->view_dispatcher, CustomActionsRunEventError);
    }
}

// Popup timeout callback — fires after the "Done!" popup expires
static void custom_actions_run_popup_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, CustomActionsRunEventPopupTimeout);
}

// ---------------------------------------------------------------------------
// Extract the filename stem from a full path for display
// e.g. "/ext/badusb/my_script.txt"  →  "my_script.txt"
// ---------------------------------------------------------------------------

static const char* path_basename(const char* path) {
    const char* last_slash = strrchr(path, '/');
    return last_slash ? last_slash + 1 : path;
}

// ---------------------------------------------------------------------------
// Popups
// ---------------------------------------------------------------------------

// "Running" popup (no timeout, Back = stop)
static void show_running_popup(Hid* app) {
    popup_reset(app->popup);
    popup_set_header(app->popup, path_basename(app->pending_script_path),
                     64, 3, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Running...\nPress Back to stop.", 64, 28,
                   AlignCenter, AlignTop);
    // No timeout — stays up until runner finishes or user presses Back
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);
}

// "Connecting" popup shown while we wait for the host (connect-per-run mode)
static void show_connecting_popup(Hid* app) {
    popup_reset(app->popup);
    popup_set_header(app->popup, path_basename(app->pending_script_path),
                     64, 3, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Connecting...\nPress Back to cancel.", 64, 28,
                   AlignCenter, AlignTop);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);
}

// Shown when the host never connects within the wait window (Back to dismiss)
static void show_connect_failed_popup(Hid* app) {
    popup_reset(app->popup);
    popup_set_header(app->popup, "Not Connected", 64, 3, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Host didn't connect.\nPress Back.", 64, 28,
                   AlignCenter, AlignTop);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);
}

// Start the script runner and show the running popup.
static void start_run(Hid* app) {
    ducky_runner_start(app->ducky_runner, app->ble_hid_profile, app->pending_script_path);
    show_running_popup(app);
}

// ---------------------------------------------------------------------------
// Scene handlers
// ---------------------------------------------------------------------------

void bt_remotes_scene_custom_actions_run_on_enter(void* context) {
    Hid* app = context;

    ducky_runner_set_callback(app->ducky_runner, custom_actions_run_cb, app);

    if(app->ducky_connect_per_run && !app->connected) {
        // Connect-per-run: bring BLE up (if needed) and wait for the host to connect
        // before firing the script. The connect_wait_timer polls app->connected.
        if(!app->ble_started) bt_remotes_start_ble(app);
        app->connect_wait_attempts = 0;
        app->connect_settle_ticks  = 0;
        show_connecting_popup(app);
        furi_timer_start(app->connect_wait_timer, CONNECT_WAIT_POLL_MS);
    } else {
        // Already connected, or per-run disabled: run immediately (legacy behavior).
        start_run(app);
    }
}

bool bt_remotes_scene_custom_actions_run_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        // Stop the wait poll and the runner, then let the scene manager pop us.
        // on_exit drops BLE (connect-per-run mode).
        furi_timer_stop(app->connect_wait_timer);
        ducky_runner_stop(app->ducky_runner);
        return false;
    }

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;

        if(event.event == BT_REMOTES_EVENT_CONNECT_TICK) {
            // Waiting for the host while connect-per-run brings BLE up.
            if(app->connected) {
                // Link is up — but the host still needs to finish HID discovery and
                // subscribe to report notifications. Settle for the per-profile
                // Connect Delay before sending, or the first keystrokes drop.
                // STEP == poll period, so ms maps exactly to whole ticks.
                // Check BEFORE incrementing: the connection callback posts one
                // immediate CONNECT_TICK on link-up, so settle_ticks=0 fires on
                // that first event without waiting a full poll period (~150 ms).
                uint8_t settle_ticks =
                    (uint8_t)(app->ducky_connect_settle_ms / CONNECT_WAIT_POLL_MS);
                if(app->connect_settle_ticks >= settle_ticks) {
                    furi_timer_stop(app->connect_wait_timer);
                    start_run(app);
                } else {
                    app->connect_settle_ticks++;
                }
            } else {
                // Lost the link before settling — restart the settle on reconnect.
                app->connect_settle_ticks = 0;
                if(++app->connect_wait_attempts >= CONNECT_WAIT_MAX_ATTEMPTS) {
                    furi_timer_stop(app->connect_wait_timer);
                    show_connect_failed_popup(app);
                }
            }

        } else if(event.event == CustomActionsRunEventDone) {
            popup_reset(app->popup);
            popup_set_header(app->popup, "Done!", 64, 22, AlignCenter, AlignCenter);
            popup_set_timeout(app->popup, 200);
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, custom_actions_run_popup_cb);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);

        } else if(event.event == CustomActionsRunEventError) {
            popup_reset(app->popup);
            popup_set_header(app->popup, "Error", 64, 3, AlignCenter, AlignTop);
            popup_set_text(
                app->popup,
                ducky_runner_get_error(app->ducky_runner),
                64, 22, AlignCenter, AlignTop);
            // No timeout — user must press Back to dismiss
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, NULL);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);

        } else if(event.event == CustomActionsRunEventPopupTimeout) {
            scene_manager_previous_scene(app->scene_manager);
        }
    }

    return consumed;
}

void bt_remotes_scene_custom_actions_run_on_exit(void* context) {
    Hid* app = context;
    // Ensure the wait poll and runner are stopped if we exit unexpectedly.
    furi_timer_stop(app->connect_wait_timer);
    ducky_runner_stop(app->ducky_runner);
    // Connect-per-run: the connection existed only for this run — drop it now.
    if(app->ducky_connect_per_run) bt_remotes_stop_ble(app);
    popup_reset(app->popup);
}
