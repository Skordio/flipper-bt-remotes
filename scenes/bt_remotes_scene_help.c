#include "../bt_remotes.h"

// ---------------------------------------------------------------------------
// Help — a menu of topics; selecting one shows a full-screen scrollable page.
// Topic list reuses app->submenu (HidViewSubmenu); page bodies are shown in a
// scrollable Widget (HidViewHelp).  Back from a page returns to the topic list;
// Back from the list returns to Profile Select.
//
// Scope: this Help documents only what this app ADDS on top of the stock
// Bluetooth Remote app - it does not re-document the default remote types.
// ---------------------------------------------------------------------------

// Short labels for the topic menu (parallel to help_pages[]).
//
// Philosophy: the main Help is a high-level overview. Every settings screen has
// its own Help page that goes into the detail - don't shadow-document those
// here.
static const char* const help_topics[] = {
    "Overview",
    "Profiles & Setup",
    "Per-Remote Tweaks",
    "Ducky Scripts & Collections",
    "Custom Gestures",
};

// Full scrollable page bodies (parallel to help_topics[]).  The Widget text
// element word-wraps to the 128px width and scrolls with Up/Down.
static const char* const help_pages[] = {
    // 0: Overview
    "OVERVIEW\n"
    "Turns your Flipper into a Bluetooth keyboard, mouse, and media remote.\n\n"
    "What this app adds over stock Bluetooth Remote:\n"
    "- Profiles: save many devices\n"
    "- Extra remote types\n"
    "- Per-remote tweaks\n"
    "- Ducky/BadUSB scripts over Bluetooth\n"
    "- Script collections, pinnable to the Start menu\n"
    "- Custom Gestures: scripting for mouse/keyboard macros\n\n"
    "To start: Profile Select > '+ New Profile', name it, then pair from your "
    "device's Bluetooth.\n\n"
    "HOLD Back to leave any remote.",

    // 1: Profiles & Setup
    "PROFILES & SETUP\n"
    "A profile is one saved device, with its own pairing, Bluetooth name, menu "
    "layout, and settings. Keep one per computer/phone and switch between them "
    "without re-pairing.\n\n"
    "- Tap a profile to connect.\n"
    "- HOLD OK to reorder.\n"
    "- Rename / Delete: Profile Settings > Profile Management.\n\n"
    "REMOTE MENU (after you pick a profile)\n"
    "- HOLD OK to reorder; OK/Back to drop.\n"
    "- Hide / Reset Order: Profile Settings > Start Menu Layout.\n\n"
    "CONNECTION\n"
    "Bluetooth Name, Delay Connect, Save BT Keys, Bluetooth Unpairing live under "
    "Profile Settings > Connection. Open its Help page for details.",

    // 2: Per-Remote Tweaks
    "PER-REMOTE TWEAKS\n"
    "Some remotes have per-profile knobs under Profile Settings > Per-Remote "
    "Settings:\n"
    "- Keynote\n"
    "- Media\n"
    "- TikTok / YT Shorts\n"
    "- DuckyScript\n\n"
    "Each has its own Help page with the details.",

    // 3: Ducky Scripts & Collections
    "DUCKY SCRIPTS & COLLECTIONS\n"
    "Runs BadUSB/Ducky .txt scripts over Bluetooth. Pick one to run; press Back "
    "to stop.\n\n"
    "Collections group scripts together - create/edit/delete under Ducky "
    "Scripts > Collections. Pin a collection to put it on the Start menu for "
    "one-tap access.",

    // 4: Custom Gestures
    "CUSTOM GESTURES\n"
    "Reusable quick actions - a sequence of mouse moves, taps, drags, scrolls, "
    "waits and keystrokes that navigate a device with one press.\n\n"
    "Gestures are plain .gesture text files edited on your PC and copied to "
    "/ext/apps_data/bt_remotes/gestures/ on the SD card. See "
    "WRITING_GESTURES.md in the app docs for the command reference.\n\n"
    "Open Custom Gestures to run a gesture or Pin it to the Start menu for "
    "one-tap access.",
};

#define BT_REMOTES_HELP_TOPIC_COUNT (sizeof(help_topics) / sizeof(help_topics[0]))

// Remembers the topic the user last had under the cursor in the list view so
// re-entering Help from Profile Select lands on that topic again.
// (scene_state for Help is already used for the page-view state machine —
//   0 == showing list, N >= 1 == viewing page N-1 — so we cannot reuse it.)
static uint32_t bt_remotes_help_last_cursor = 0;

static void bt_remotes_scene_help_submenu_cb(void* context, uint32_t index) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

static void bt_remotes_scene_help_build_menu(Hid* app) {
    submenu_reset(app->submenu);
    submenu_set_header(app->submenu, "Help");
    for(uint32_t i = 0; i < BT_REMOTES_HELP_TOPIC_COUNT; i++) {
        submenu_add_item(
            app->submenu, help_topics[i], i, bt_remotes_scene_help_submenu_cb, app);
    }
}

static void bt_remotes_scene_help_show_page(Hid* app, uint32_t topic) {
    widget_reset(app->help_widget);
    widget_add_text_scroll_element(app->help_widget, 0, 0, 128, 64, help_pages[topic]);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewHelp);
}

void bt_remotes_scene_help_on_enter(void* context) {
    Hid* app = context;
    scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneHelp, 0);
    bt_remotes_scene_help_build_menu(app);
    submenu_set_selected_item(app->submenu, bt_remotes_help_last_cursor);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
}

bool bt_remotes_scene_help_on_event(void* context, SceneManagerEvent event) {
    Hid* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        // A topic was chosen from the list -> show its scrollable page.
        if(event.event < BT_REMOTES_HELP_TOPIC_COUNT) {
            scene_manager_set_scene_state(
                app->scene_manager, BtRemotesSceneHelp, event.event + 1);
            bt_remotes_scene_help_show_page(app, event.event);
            consumed = true;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        uint32_t state =
            scene_manager_get_scene_state(app->scene_manager, BtRemotesSceneHelp);
        if(state != 0) {
            // Viewing a page -> return to the topic list, cursor on that topic.
            uint32_t topic = state - 1;
            bt_remotes_scene_help_build_menu(app);
            submenu_set_selected_item(app->submenu, topic);
            view_dispatcher_switch_to_view(app->view_dispatcher, HidViewSubmenu);
            scene_manager_set_scene_state(app->scene_manager, BtRemotesSceneHelp, 0);
            consumed = true;
        }
        // In list mode: leave unconsumed so the scene manager pops to Profile Select.
    }

    return consumed;
}

void bt_remotes_scene_help_on_exit(void* context) {
    Hid* app = context;
    // Save cursor before reset so re-entering Help lands on the same topic.
    bt_remotes_help_last_cursor = submenu_get_selected_item(app->submenu);
    submenu_reset(app->submenu);
    widget_reset(app->help_widget);
}
