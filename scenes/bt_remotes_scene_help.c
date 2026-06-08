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
static const char* const help_topics[] = {
    "Overview",
    "Getting Started",
    "Profiles",
    "Pairing & Bluetooth",
    "The Remote Menu",
    "Per-Remote Changes",
    "Ducky Scripts & Collections",
    "Custom Gestures",
    "Settings Reference",
    "Tips & Exiting",
};

// Full scrollable page bodies (parallel to help_topics[]).  The Widget text
// element word-wraps to the 128px width and scrolls with Up/Down.
static const char* const help_pages[] = {
    // 0: Overview
    "OVERVIEW\n"
    "Turns your Flipper into a Bluetooth keyboard, mouse, and media remote.\n\n"
    "What this app adds over the stock Bluetooth Remote:\n"
    "- Profiles: save many devices\n"
    "- Extra remote types\n"
    "- Per-remote settings & tweaks\n"
    "- Run Ducky/BadUSB scripts over Bluetooth\n"
    "- Script collections, pinnable to the menu\n\n"
    "New? See 'Getting Started'.",

    // 1: Getting Started
    "GETTING STARTED\n"
    "1. Profile Select > '+ New Profile', name it.\n"
    "2. The Flipper starts Bluetooth.\n"
    "3. Pair with it from your device's Bluetooth settings.\n"
    "4. Pick a remote and use the buttons.\n\n"
    "HOLD Back to leave a remote.",

    // 2: Profiles
    "PROFILES\n"
    "A profile is one saved device, with its own pairing, Bluetooth name, menu "
    "layout, and settings.\n\n"
    "Keep one per computer/phone and switch between them without re-pairing.\n\n"
    "- Tap a profile to connect.\n"
    "- HOLD OK to reorder the list.\n"
    "- Rename / Delete live in Settings.",

    // 3: Pairing & Bluetooth
    "PAIRING & BLUETOOTH\n"
    "Pair once from your device's Bluetooth settings; it saves automatically.\n\n"
    "Bluetooth Name: the name your device sees.\n\n"
    "Bluetooth Unpairing: gives the profile a brand-new identity and forgets "
    "all pairings. Use it if a device won't reconnect, then pair fresh.\n\n"
    "Delay Connect (Settings): when On, this profile turns Bluetooth on only "
    "while you're in a remote, script, or gesture, and disconnects when you go "
    "back to the menu. Off (default): it connects as soon as you pick the "
    "profile and stays connected. Use On to keep the Flipper off the air until "
    "you actually start using a remote.",

    // 4: The Remote Menu
    "THE REMOTE MENU\n"
    "The list of remotes shown after you pick a profile.\n\n"
    "- Reorder: HOLD OK, move with Up/Down, OK/Back to drop.\n"
    "- Hide: Settings > Hide Remote Types ([x] shown, [ ] hidden).\n"
    "- Reset Menu Order restores defaults.\n\n"
    "Saved per profile.",

    // 5: Per-Remote Changes
    "PER-REMOTE CHANGES\n"
    "Some of the original remotes gain extra behavior here, set in\n"
    "Settings > Per-Remote Settings (per profile). More options will be added "
    "over time.\n\n"
    "KEYNOTE\n"
    "Pick what a short Back press sends: Delete, Left Arrow, Escape, or None - "
    "to match different slide apps.\n\n"
    "MEDIA\n"
    "- Mode: Legacy (Left/Right skip tracks) or Improved (TAP Left/Right to "
    "seek within a video, HOLD to skip tracks).\n"
    "- Mouse Switcher: when On, a short Back opens a mouse view; a short Back "
    "there returns to Media. Hold Back still exits.\n\n"
    "TIKTOK / YT SHORTS\n"
    "- Scroll Mode: Wheel (scroll-wheel) or Gesture (emulated finger swipe) for "
    "moving Up/Down between videos.\n"
    "- In Gesture mode you can tune the swipe for your phone: Inward Margin (how "
    "far in from the side it starts), Edge Margin (how far off the top/bottom "
    "edge before holding), and Swipe Length (drag distance while held).\n\n"
    "DUCKYSCRIPT\n"
    "- Connect Per Run: stay disconnected while browsing scripts; connect only for "
    "each script run, then disconnect.\n"
    "- Connect Delay: how long to wait after connecting before sending keys (raise "
    "it if the first keys of a run are dropped).",

    // 6: Ducky Scripts & Collections
    "DUCKY SCRIPTS & COLLECTIONS\n"
    "Ducky Scripts runs BadUSB/Ducky .txt scripts over Bluetooth. Pick one to "
    "run it; press Back to stop.\n\n"
    "Collections group scripts together - create, edit, and delete them under "
    "Ducky Scripts > Collections.\n\n"
    "Pin a collection to put it on the main menu for one-tap access.",

    // 7: Custom Gestures
    "CUSTOM GESTURES\n"
    "Build reusable 'quick actions' - a sequence of mouse moves, taps, drags, "
    "scrolls, waits and keystrokes that navigate a device (e.g. deep into iOS "
    "Settings) with one press.\n\n"
    "Open Custom Gestures from the main menu, Create one, then Add lines. Each "
    "line is one command; the editor's Help lists them all.\n\n"
    "Commands: anchor, move, tap, click, drag, scroll, wait, key, type. Start "
    "with 'anchor' so moves are repeatable.\n\n"
    "Run a gesture from its menu, or Pin it to the main menu for one-tap access.",

    // 8: Settings Reference
    "SETTINGS REFERENCE\n"
    "Bluetooth Name - name shown to the device.\n"
    "Vibration - buzz on connect/disconnect.\n"
    "Hide Remote Types - show/hide menu items.\n"
    "Per-Remote Settings - per-remote options.\n"
    "Delay Connect - only connect while in a remote.\n"
    "Reset Menu Order - restore defaults.\n"
    "Rename Profile - rename the profile.\n"
    "Bluetooth Unpairing - new identity, forget pairings.\n"
    "Save Profile - snapshot pairing.\n"
    "Delete Profile - remove the profile.\n\n"
    "Most apply to the active profile.",

    // 9: Tips & Exiting
    "TIPS & EXITING\n"
    "- HOLD Back to leave any remote.\n"
    "- Vibration can confirm connect/disconnect.\n"
    "- Opening Settings briefly drops Bluetooth; it reconnects when you go "
    "Back.\n"
    "- Each profile keeps its own layout and settings.\n"
    "- Reach this Help anytime from Profile Select.",
};

#define BT_REMOTES_HELP_TOPIC_COUNT (sizeof(help_topics) / sizeof(help_topics[0]))

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
    submenu_reset(app->submenu);
    widget_reset(app->help_widget);
}
