#include "../bt_remotes.h"
#include <flipper_format/flipper_format.h>

// Two-phase Create Shortcut flow (mirrors profile_new's mid-scene view switch):
//   Phase 1 (on_enter):  submenu picker — "Profile Only" plus every fixed
//                        Start-menu item except Settings. The pick index is
//                        sent as a custom event.
//   Phase 2 (on_event):  write the .btremote to BT_REMOTES_LAUNCHER_DIR
//                        (adding a Remote: label line when a remote was
//                        picked), then show a Saved!/Error popup that pops
//                        back on timeout.
// The file can be moved anywhere on the SD card afterwards — the firmware
// archive routes .btremote on extension, not location.

// Submenu row 0 = profile-only; rows 1..15 = bt_remotes_menu_default[row-1];
// rows 16.. = pinned item (collection or gesture) at pidx = row-16. EventDone
// sits above any possible pick (fixed 0..15 + pins 16..31 < 100).
enum BtRemotesShortcutCreateEvent {
    BtRemotesShortcutCreateEventPickBase = 0,
    BtRemotesShortcutCreateEventDone     = 100,
};

// target encoding (mirrors the Start-menu event space shifted by nothing):
//   BT_REMOTES_LAUNCHER_REMOTE_NONE          — profile-only shortcut
//   < BT_REMOTES_MENU_ITEM_COUNT             — fixed remote index (Remote: field)
//   >= BT_REMOTES_MENU_ITEM_COUNT            — pinned slot pidx + 16 (Pin: field)
static const char* bt_remotes_shortcut_target_name(Hid* app, uint8_t target) {
    if(target >= BT_REMOTES_MENU_ITEM_COUNT) {
        return app->pinned_collections[target - BT_REMOTES_MENU_ITEM_COUNT];
    }
    return bt_remotes_menu_default[target].label;
}

static bool bt_remotes_shortcut_write(Hid* app, uint8_t target) {
    FuriString* path = furi_string_alloc_printf(
        "%s/%s", BT_REMOTES_LAUNCHER_DIR, app->active_profile);
    if(target != BT_REMOTES_LAUNCHER_REMOTE_NONE) {
        // " - <name>" suffix keeps target shortcuts from overwriting the
        // profile-only one. Labels can contain filesystem-hostile chars
        // ("TikTok / YT Shorts" has '/'), so sanitize the copy.
        furi_string_cat_str(path, " - ");
        for(const char* c = bt_remotes_shortcut_target_name(app, target); *c; c++) {
            furi_string_push_back(path, strchr("<>:\"/\\|?*", *c) ? '-' : *c);
        }
    }
    furi_string_cat_str(path, BT_REMOTES_LAUNCHER_EXT);

    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    bool           ok  = false;
    if(flipper_format_file_open_always(fff, furi_string_get_cstr(path))) {
        if(flipper_format_write_header_cstr(
               fff, BT_REMOTES_LAUNCHER_FILETYPE, BT_REMOTES_LAUNCHER_VERSION)) {
            ok = flipper_format_write_string_cstr(fff, "Profile", app->active_profile);
        }
        if(ok && target != BT_REMOTES_LAUNCHER_REMOTE_NONE) {
            ok = flipper_format_write_string_cstr(
                fff,
                (target >= BT_REMOTES_MENU_ITEM_COUNT) ? "Pin" : "Remote",
                bt_remotes_shortcut_target_name(app, target));
        }
    }
    flipper_format_file_close(fff);
    flipper_format_free(fff);
    furi_string_free(path);
    return ok;
}

static void bt_remotes_scene_profile_shortcut_create_submenu_cb(
    void* context,
    uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void bt_remotes_scene_profile_shortcut_create_popup_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(
        app->view_dispatcher, BtRemotesShortcutCreateEventDone);
}

void bt_remotes_scene_profile_shortcut_create_on_enter(void* context) {
    Hid* app = context;

    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Shortcut Target");

    submenu_add_item(
        app->submenu,
        "Profile Only",
        BtRemotesShortcutCreateEventPickBase,
        bt_remotes_scene_profile_shortcut_create_submenu_cb,
        app);
    // Every fixed item except Settings (always last in the enum).
    for(uint8_t i = 0; i < BT_REMOTES_MENU_ITEM_COUNT - 1; i++) {
        submenu_add_item(
            app->submenu,
            bt_remotes_menu_default[i].label,
            BtRemotesShortcutCreateEventPickBase + 1 + i,
            bt_remotes_scene_profile_shortcut_create_submenu_cb,
            app);
    }
    // Pinned collections / gestures (event = MENU_ITEM_COUNT + pidx, matching
    // the target encoding decoded in on_event).
    for(uint8_t pidx = 0; pidx < app->pinned_count; pidx++) {
        submenu_add_item(
            app->submenu,
            app->pinned_collections[pidx],
            BT_REMOTES_MENU_ITEM_COUNT + pidx,
            bt_remotes_scene_profile_shortcut_create_submenu_cb,
            app);
    }

    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_profile_shortcut_create_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;

        if(event.event == BtRemotesShortcutCreateEventDone) {
            scene_manager_previous_scene(app->scene_manager);

        } else {
            // Pick: 0 = profile only, 1..15 = remote index + 1,
            // >= 16 = pinned slot pidx + 16 (passed through unchanged).
            uint8_t target =
                (event.event == 0) ? BT_REMOTES_LAUNCHER_REMOTE_NONE :
                (event.event < BT_REMOTES_MENU_ITEM_COUNT) ? (uint8_t)(event.event - 1) :
                                                             (uint8_t)event.event;
            bool ok = bt_remotes_shortcut_write(app, target);

            popup_reset(app->popup);
            if(ok) {
                // popup_set_text stores the pointer, not a copy — all strings
                // here are persistent (Hid fields / static table), never
                // transient allocations.
                popup_set_header(app->popup, "Saved!", 64, 10, AlignCenter, AlignTop);
                popup_set_text(
                    app->popup,
                    (target == BT_REMOTES_LAUNCHER_REMOTE_NONE) ?
                        app->active_profile :
                        bt_remotes_shortcut_target_name(app, target),
                    64,
                    28,
                    AlignCenter,
                    AlignTop);
            } else {
                popup_set_header(app->popup, "Error", 64, 10, AlignCenter, AlignTop);
                popup_set_text(
                    app->popup, "Could not write shortcut.", 64, 28, AlignCenter, AlignTop);
            }
            popup_set_timeout(app->popup, 2000);
            popup_set_context(app->popup, app);
            popup_set_callback(app->popup, bt_remotes_scene_profile_shortcut_create_popup_cb);
            popup_enable_timeout(app->popup);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);
        }
    }

    return consumed;
}

void bt_remotes_scene_profile_shortcut_create_on_exit(void* context) {
    Hid* app = context;
    submenu_reset(app->submenu);
    popup_reset(app->popup);
}
