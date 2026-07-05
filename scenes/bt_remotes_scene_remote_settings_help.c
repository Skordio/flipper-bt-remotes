#include "../bt_remotes.h"

// Shared in-app Help screen for the Per-Remote Settings pages. The calling
// settings scene sets this scene's state to a RemoteSettingsHelpTopic before
// pushing it; on_enter renders the matching scrollable page into the shared
// help_widget (HidViewHelp). Back pops straight back to the settings page.
//
// Adding a per-page Help screen is the norm for new Per-Remote Settings pages:
// add a topic + text entry here and a "Help" item that pushes this scene with
// the topic as scene state.

// RemoteSettingsHelpTopic is defined in bt_remotes.h so the settings scenes can
// reference the topic values.
//
// Parallel to RemoteSettingsHelpTopic. Word-wraps to 128px and scrolls Up/Down.
static const char* const remote_settings_help_pages[] = {
    // Keynote
    "Back Button: choose what a short press of the Flipper Back button sends, to "
    "match different slide apps:\n"
    "- Delete\n"
    "- Left Arrow\n"
    "- Escape\n"
    "- None (no key)\n\n"
    "Hold Back always exits the remote.",

    // Media
    "Mode:\n"
    "- Legacy: Left/Right skip to the previous/next track.\n"
    "- Improved: TAP Left/Right to seek within the current track, HOLD Left/Right "
    "to skip tracks.\n\n"
    "Mouse Switcher:\n"
    "- On: a short Back opens a mouse view; a short Back there returns to Media.\n"
    "- Off: short Back is unused.\n"
    "Hold Back always exits.",

    // TikTok
    "Scroll Mode:\n"
    "- Wheel: Up/Down send scroll-wheel events.\n"
    "- Gesture: Up/Down emulate a finger swipe (mouse click-drag).\n\n"
    "The values below tune Gesture mode (px):\n"
    "- Inward Margin: how far in from the side edge the cursor moves before the "
    "swipe.\n"
    "- Edge Margin: how far off the top/bottom edge it travels before holding the "
    "button.\n"
    "- Swipe Length: drag distance while the button is held.",

    // DuckyScript
    "Connect Per Run:\n"
    "- On: stay disconnected while browsing scripts; Bluetooth connects only for "
    "the duration of each script run, then disconnects.\n"
    "- Off: scripts run on the existing connection (default).\n\n"
    "Connect Delay:\n"
    "How long to wait after the link comes up before sending keystrokes, so the "
    "host finishes connecting first. If the first keys of a run are dropped, raise "
    "it; if it feels slow, lower it. Only used when Connect Per Run is On.",

    // Global Settings
    "Settings that apply to the app as a whole - not to any one profile.\n\n"
    "Default Bluetooth Name:\n"
    "The Bluetooth name applied to newly-created profiles. Existing profiles keep "
    "their own name (change that under Profile Settings > Connection).\n\n"
    "Vibration:\n"
    "- Neither: no buzz.\n"
    "- Connect: buzz when a device connects.\n"
    "- Disconnect: buzz when a device disconnects.\n"
    "- Both: buzz on connect AND disconnect.",

    // Connection
    "Bluetooth name and connection behavior for the active profile.\n\n"
    "Bluetooth Name:\n"
    "The name your device sees when pairing. Per-profile - this is separate "
    "from the Default Bluetooth Name in Global Settings, which is the template "
    "applied to NEW profiles.\n\n"
    "Reset Name to Default:\n"
    "Replaces this profile's Bluetooth Name with the Default Bluetooth Name "
    "from Global Settings and re-advertises. Handy after editing the global "
    "default if you want existing profiles to pick it up.\n\n"
    "Delay Connect:\n"
    "- On: Bluetooth is off at the menu and only turns on while you're inside a "
    "remote, script, or gesture. Useful to keep the Flipper off the air.\n"
    "- Off (default): connects as soon as you open the profile and stays "
    "connected.\n\n"
    "Save BT Keys:\n"
    "Snapshots the current pairing into the profile's stored keys. Pairings "
    "normally save on their own - use this only if a host won't reconnect after "
    "a reboot.\n\n"
    "Bluetooth Unpairing:\n"
    "Gives the profile a brand-new identity and forgets all of its pairings. "
    "Use it if a device won't reconnect, then re-pair fresh.",

    // Start Menu Layout
    "Customize the remote list shown after you pick this profile. Saved "
    "per-profile.\n\n"
    "Hide Remote Types:\n"
    "Toggle which remote items appear on the Start menu. [x] = shown, [ ] = "
    "hidden. Settings is always visible (cannot be hidden).\n\n"
    "Reset Menu Order:\n"
    "Restores the default order AND makes every item visible again. Use this "
    "to undo a HOLD-OK reorder or to bring back hidden items.",

    // Per-Remote Settings (hub)
    "Per-profile knobs specific to individual remote types. Each has its own "
    "Help page with the full details:\n\n"
    "Keynote:\n"
    "Which key short Back sends (Delete / Left Arrow / Escape / None) to match "
    "different slide apps.\n\n"
    "Media:\n"
    "Mode (Legacy vs Improved) and Mouse Switcher (short Back swaps to a mouse "
    "view).\n\n"
    "TikTok / YT Shorts:\n"
    "Scroll Mode (Wheel or emulated finger-swipe Gesture). In Gesture mode, "
    "Inward Margin / Edge Margin / Swipe Length tune the swipe for your phone.\n\n"
    "DuckyScript:\n"
    "Connect Per Run (stay disconnected while browsing scripts) and Connect "
    "Delay (settle time after the link is up).",

    // Profile Management
    "Manage the active profile's identity on disk.\n\n"
    "Rename Profile:\n"
    "Changes the profile's filename and the label shown on Profile Select. "
    "Existing pairings are preserved.\n\n"
    "Create Shortcut:\n"
    "Writes a .btremote file to /ext/apps_data/bt_remotes/launchers/ that "
    "opens this profile directly from File Browser or Favorites. Move the "
    "file anywhere — routing is by extension. Rename the profile and the "
    "shortcut breaks.\n\n"
    "Delete Profile:\n"
    "Removes the profile, its menu layout, its per-remote settings, and its "
    "stored pairings. Cannot be undone. Returns you to Profile Select.",
};

void bt_remotes_scene_remote_settings_help_on_enter(void* context) {
    Hid*     app   = context;
    uint32_t topic = scene_manager_get_scene_state(
        app->scene_manager, BtRemotesSceneRemoteSettingsHelp);
    widget_reset(app->help_widget);
    widget_add_text_scroll_element(
        app->help_widget, 0, 0, 128, 64, remote_settings_help_pages[topic]);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewHelp);
}

bool bt_remotes_scene_remote_settings_help_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    // Back pops to the calling settings scene; nothing else to handle.
    return false;
}

void bt_remotes_scene_remote_settings_help_on_exit(void* context) {
    Hid* app = context;
    widget_reset(app->help_widget);
}
