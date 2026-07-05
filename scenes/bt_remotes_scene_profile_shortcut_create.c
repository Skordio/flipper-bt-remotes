#include "../bt_remotes.h"
#include <flipper_format/flipper_format.h>

// Writes a .btremote launcher shortcut for the active profile to
// BT_REMOTES_LAUNCHER_DIR, then shows a confirmation popup that pops back on
// timeout. Mirrors the structure of scene_reset_bt_name / scene_save_profile:
// do the work in on_enter, popup handles the rest. The file can be moved
// anywhere on the SD card afterwards — the firmware archive routes .btremote
// on extension, not location.

enum BtRemotesShortcutCreateEvent {
    BtRemotesShortcutCreateEventDone,
};

static bool bt_remotes_shortcut_write(Hid* app) {
    FuriString* path = furi_string_alloc_printf(
        "%s/%s%s",
        BT_REMOTES_LAUNCHER_DIR,
        app->active_profile,
        BT_REMOTES_LAUNCHER_EXT);
    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    bool           ok  = false;
    if(flipper_format_file_open_always(fff, furi_string_get_cstr(path))) {
        if(flipper_format_write_header_cstr(
               fff, BT_REMOTES_LAUNCHER_FILETYPE, BT_REMOTES_LAUNCHER_VERSION)) {
            ok = flipper_format_write_string_cstr(fff, "Profile", app->active_profile);
        }
    }
    flipper_format_file_close(fff);
    flipper_format_free(fff);
    furi_string_free(path);
    return ok;
}

static void bt_remotes_scene_profile_shortcut_create_popup_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, BtRemotesShortcutCreateEventDone);
}

void bt_remotes_scene_profile_shortcut_create_on_enter(void* context) {
    Hid* app = context;

    bool ok = bt_remotes_shortcut_write(app);

    popup_reset(app->popup);
    if(ok) {
        // popup_set_text stores the pointer, not a copy — use app->active_profile
        // (a persistent Hid field) rather than a transient allocation, and skip
        // the full path (which would overflow the 128px screen anyway).
        popup_set_header(app->popup, "Saved!", 64, 10, AlignCenter, AlignTop);
        popup_set_text(app->popup, app->active_profile, 64, 28, AlignCenter, AlignTop);
    } else {
        popup_set_header(app->popup, "Error", 64, 10, AlignCenter, AlignTop);
        popup_set_text(app->popup, "Could not write shortcut.", 64, 28, AlignCenter, AlignTop);
    }
    popup_set_timeout(app->popup, 2500);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, bt_remotes_scene_profile_shortcut_create_popup_cb);
    popup_enable_timeout(app->popup);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);
}

bool bt_remotes_scene_profile_shortcut_create_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == BtRemotesShortcutCreateEventDone) {
            scene_manager_previous_scene(app->scene_manager);
            consumed = true;
        }
    }

    return consumed;
}

void bt_remotes_scene_profile_shortcut_create_on_exit(void* context) {
    Hid* app = context;
    popup_reset(app->popup);
}
