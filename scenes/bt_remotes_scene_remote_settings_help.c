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
    "KEYNOTE SETTINGS\n"
    "Back Button: choose what a short press of the Flipper Back button sends, to "
    "match different slide apps:\n"
    "- Delete\n"
    "- Left Arrow\n"
    "- Escape\n"
    "- None (no key)\n\n"
    "Hold Back always exits the remote.",

    // Media
    "MEDIA SETTINGS\n"
    "Mode:\n"
    "- Legacy: Left/Right skip to the previous/next track.\n"
    "- Improved: TAP Left/Right to seek within the current track, HOLD Left/Right "
    "to skip tracks.\n\n"
    "Mouse Switcher:\n"
    "- On: a short Back opens a mouse view; a short Back there returns to Media.\n"
    "- Off: short Back is unused.\n"
    "Hold Back always exits.",

    // TikTok
    "TIKTOK SETTINGS\n"
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
    "DUCKYSCRIPT SETTINGS\n"
    "Connect Per Run:\n"
    "- On: stay disconnected while browsing scripts; Bluetooth connects only for "
    "the duration of each script run, then disconnects.\n"
    "- Off: scripts run on the existing connection (default).\n\n"
    "Connect Delay:\n"
    "How long to wait after the link comes up before sending keystrokes, so the "
    "host finishes connecting first. If the first keys of a run are dropped, raise "
    "it; if it feels slow, lower it. Only used when Connect Per Run is On.",

    // Global Settings
    "GLOBAL SETTINGS\n"
    "Settings that apply to the app as a whole - not to any one profile.\n\n"
    "Default Bluetooth Name:\n"
    "The Bluetooth name applied to newly-created profiles. Existing profiles keep "
    "their own name (change that under Profile Settings > Connection).\n\n"
    "Vibration:\n"
    "- Neither: no buzz.\n"
    "- Connect: buzz when a device connects.\n"
    "- Disconnect: buzz when a device disconnects.\n"
    "- Both: buzz on connect AND disconnect.",

    // Profile Settings
    "PROFILE SETTINGS\n"
    "Settings for the active profile, grouped into sections.\n\n"
    "Connection:\n"
    "Bluetooth name shown to the host, Delay Connect (only run Bluetooth while "
    "inside a remote), Save BT Keys (snapshot the current pairing - use if a host "
    "won't reconnect across reboots), and Bluetooth Unpairing (forget all pairings "
    "and start with a fresh identity).\n\n"
    "Start Menu Layout:\n"
    "Hide remote types from the menu and reset menu order to defaults.\n\n"
    "Per-Remote Settings:\n"
    "Tweaks for individual remote types (Keynote, Media, TikTok, DuckyScript).\n\n"
    "Profile Management:\n"
    "Rename or delete the profile.",
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
