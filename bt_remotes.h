#pragma once

#include <furi.h>
#include <furi_hal_bt.h>
#include <furi_hal_usb.h>
#include <furi_hal_usb_hid.h>

#include "helpers/ble_hid_ext_profile.h"

#include <bt/bt_service/bt.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <notification/notification.h>
#include <storage/storage.h>

#include <gui/modules/submenu.h>
#include <gui/modules/variable_item_list.h>
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include <gui/modules/file_browser.h>
#include <gui/modules/widget.h>
#include "views/hid_remote_menu.h"
#include "helpers/ducky_runner.h"
#include "helpers/gesture_runner.h"
#include "views/hid_keynote.h"
#include "views/hid_keyboard.h"
#include "views/hid_numpad.h"
#include "views/hid_media.h"
#include "views/hid_music_macos.h"
#include "views/hid_movie.h"
#include "views/hid_mouse.h"
#include "views/hid_mouse_clicker.h"
#include "views/hid_mouse_jiggler.h"
#include "views/hid_mouse_jiggler_stealth.h"
#include "views/hid_tiktok.h"
#include "views/hid_ptt.h"
#include "views/hid_ptt_menu.h"

#include "views.h"
#include "scenes/bt_remotes_scene.h"

#define HID_BT_KEYS_STORAGE_NAME ".bt_hid.keys"

#define BT_REMOTES_PROFILE_NAME_LEN  32
#define BT_REMOTES_PROFILE_MAX_COUNT 16
#define BT_REMOTES_MENU_ITEM_COUNT   16
#define BT_REMOTES_PINNED_MAX        16
#define BT_REMOTES_MENU_ORDER_LEN    (BT_REMOTES_MENU_ITEM_COUNT + BT_REMOTES_PINNED_MAX)

// Previous on-disk menu layout (before Custom Gestures was added), used to
// migrate saved menu_order arrays. Adding the fixed item moved the fixed/pinned
// boundary 15 -> 16, so old pinned-slot values must be shifted +1 on load.
#define BT_REMOTES_MENU_ITEM_COUNT_V1 15
#define BT_REMOTES_MENU_ORDER_LEN_V1  31

#define BT_REMOTES_PROFILES_DIR APP_DATA_PATH("profiles")
#define BT_REMOTES_CFG_PATH     APP_DATA_PATH(".bt_hid.cfg")
#define BT_REMOTES_APP_CFG_PATH APP_DATA_PATH("app.cfg")
#define BT_REMOTES_CFG_EXT      ".cfg"
#define BT_REMOTES_KEYS_EXT     ".keys"

// Ducky Script Collections
#define BT_REMOTES_COLLECTION_DIR        APP_DATA_PATH("collections")
#define BT_REMOTES_COLLECTION_EXT        ".collection"
#define BT_REMOTES_COLLECTION_NAME_LEN   32
#define BT_REMOTES_COLLECTION_MAX        16
#define BT_REMOTES_COLLECTION_SCRIPT_MAX 16

// Custom Gestures (global library; GESTURE_LINE_MAX/_LEN live in gesture_runner.h)
#define BT_REMOTES_GESTURE_DIR      APP_DATA_PATH("gestures")
#define BT_REMOTES_GESTURE_EXT      ".gesture"
#define BT_REMOTES_GESTURE_NAME_LEN 32
#define BT_REMOTES_GESTURE_MAX      32

// Keynote back-button key options (short press of Flipper Back button)
typedef enum {
    KeynoteBackKeyDelete    = 0, // HID_KEYBOARD_DELETE (default, current behavior)
    KeynoteBackKeyLeftArrow = 1, // HID_KEYBOARD_LEFT_ARROW
    KeynoteBackKeyEscape    = 2, // HID_KEYBOARD_ESCAPE
    KeynoteBackKeyNone      = 3, // no keyboard key (consumer AC_BACK only)
} KeynoteBackKey;

#define KEYNOTE_BACK_KEY_DEFAULT KeynoteBackKeyDelete
#define KEYNOTE_BACK_KEY_COUNT   4

// Media remote behavior mode (per-profile)
typedef enum {
    MediaModeLegacy   = 0, // L/R = prev/next track (current behavior)
    MediaModeImproved = 1, // tap L/R = seek arrows, hold L/R = prev/next track
} MediaMode;

#define MEDIA_MODE_DEFAULT MediaModeLegacy
#define MEDIA_MODE_COUNT   2

// Media remote mouse switcher: short Back opens the mouse sub-view (independent of mode)
#define MEDIA_MOUSE_SWITCH_DEFAULT 0

// Deferred BLE start (per-profile): 0 = connect immediately on profile select and
// stay connected at the Start menu (default/legacy). 1 = only advertise/connect
// while a remote/script/gesture screen is open; disconnect on return to the menu.
#define DELAY_CONNECT_DEFAULT 0

// Ducky/Collections "connect per run" (per-profile): 0 = run scripts on the existing
// connection (default). 1 = stay disconnected while browsing scripts; connect only
// for the duration of each script run, then disconnect. Independent of delay_connect.
#define DUCKY_CONNECT_PER_RUN_DEFAULT 0

// Custom view-dispatcher event posted by the connect-wait timer so the run scene
// re-checks app->connected. High sentinel to avoid colliding with scene-local enums.
#define BT_REMOTES_EVENT_CONNECT_TICK 0xC0FFEE01u

// Ducky "connect per run": poll period + cap while the run scene waits for the host.
#define CONNECT_WAIT_POLL_MS      150
#define CONNECT_WAIT_MAX_ATTEMPTS 100 // ~15 seconds before giving up
// After the BLE link comes up, wait this long before sending HID, so the host
// finishes HID service discovery and subscribes to report notifications first.
// (Link-up fires well before the host is ready; without this the first keys are
// lost.) Per-profile, set in DuckyScript per-remote settings (ms). STEP == the poll
// period so the displayed ms maps exactly to whole poll ticks. Default ~1.05 s.
#define DUCKY_CONNECT_SETTLE_MIN     0
#define DUCKY_CONNECT_SETTLE_MAX     3000
#define DUCKY_CONNECT_SETTLE_STEP    CONNECT_WAIT_POLL_MS
#define DUCKY_CONNECT_SETTLE_DEFAULT 1050
#define DUCKY_CONNECT_SETTLE_COUNT \
    (((DUCKY_CONNECT_SETTLE_MAX) - (DUCKY_CONNECT_SETTLE_MIN)) / (DUCKY_CONNECT_SETTLE_STEP) + 1)

// TikTok / YT Shorts scroll behavior (per-profile)
typedef enum {
    TikTokScrollWheel   = 0, // scroll-wheel burst (current behavior)
    TikTokScrollGesture = 1, // emulated finger swipe (mouse click-drag)
} TikTokScrollMode;

#define TIKTOK_SCROLL_MODE_DEFAULT TikTokScrollWheel
#define TIKTOK_SCROLL_MODE_COUNT   2

// TikTok gesture-swipe tunable motion values (per-profile, px). Each is a
// Left/Right-adjustable setting; min..max in steps. Defaults equal the original
// hardcoded swipe constants, so existing profiles behave identically.
//   Inward Margin: horizontal inset before the vertical swipe (single int8 move)
#define TIKTOK_GESTURE_INSET_MIN     20
#define TIKTOK_GESTURE_INSET_MAX     300
#define TIKTOK_GESTURE_INSET_STEP    10
#define TIKTOK_GESTURE_INSET_DEFAULT 70
//   Edge Margin: vertical travel off the top/bottom edge before the button is held
#define TIKTOK_GESTURE_MARGIN_MIN     20
#define TIKTOK_GESTURE_MARGIN_MAX     300
#define TIKTOK_GESTURE_MARGIN_STEP    20
#define TIKTOK_GESTURE_MARGIN_DEFAULT 180
//   Swipe Length: drag distance while the button is held
#define TIKTOK_GESTURE_SWIPE_MIN     100
#define TIKTOK_GESTURE_SWIPE_MAX     600
#define TIKTOK_GESTURE_SWIPE_STEP    50
#define TIKTOK_GESTURE_SWIPE_DEFAULT 350
// Number of selectable values for each range (used as VariableItemList counts).
#define TIKTOK_GESTURE_VALUE_COUNT(min, max, step) (((max) - (min)) / (step) + 1)

// Topics for the shared Per-Remote Settings Help scene
// (bt_remotes_scene_remote_settings_help.c). The launching settings scene stores
// the topic as that scene's state before pushing it.
typedef enum {
    RemoteSettingsHelpKeynote = 0,
    RemoteSettingsHelpMedia   = 1,
    RemoteSettingsHelpTikTok  = 2,
    RemoteSettingsHelpDucky   = 3,
    RemoteSettingsHelpGlobal  = 4,
    RemoteSettingsHelpProfile = 5,
} RemoteSettingsHelpTopic;

// Start-menu item indices — shared by bt_remotes_scene_start.c and bt_remotes_scene_main.c.
typedef enum {
    BtRemotesStartIndexKeynote             = 0,
    BtRemotesStartIndexKeynoteVertical     = 1,
    BtRemotesStartIndexKeyboard            = 2,
    BtRemotesStartIndexNumpad              = 3,
    BtRemotesStartIndexMedia               = 4,
    BtRemotesStartIndexMusicMacOs          = 5,
    BtRemotesStartIndexMovie               = 6,
    BtRemotesStartIndexTikTok              = 7,
    BtRemotesStartIndexMouse               = 8,
    BtRemotesStartIndexMouseClicker        = 9,
    BtRemotesStartIndexMouseJiggler        = 10,
    BtRemotesStartIndexMouseJigglerStealth = 11,
    BtRemotesStartIndexPushToTalk          = 12,
    BtRemotesStartIndexCustomActions       = 13,
    BtRemotesStartIndexCustomGestures      = 14,
    // Settings stays the last fixed item (highest index) — hide_items and the
    // load-time "never hide" guard rely on it being last. Custom Gestures was
    // inserted just before it; profiles saved by the previous layout are migrated
    // in bt_remotes_profile_activate (see the menu_order V1 arm).
    BtRemotesStartIndexSettings            = 15,
} BtRemotesStartIndex;

typedef struct Hid Hid;

struct Hid {
    FuriHalBleProfileBase* ble_hid_profile;
    BleProfileHidExtParams ble_hid_cfg;
    bool ble_started;
    bool connected; // live BLE link state (set by the status callback, cleared on stop)
    Bt* bt;
    Gui* gui;
    NotificationApp* notifications;
    Storage* storage;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    VariableItemList* var_item_list; // adjustable rows for TikTok gesture settings
    DialogEx* dialog;
    TextInput* text_input;
    Popup* popup;
    Widget* help_widget; // scrollable text pages for the Help scene
    HidKeynote* hid_keynote;
    HidKeyboard* hid_keyboard;
    HidNumpad* hid_numpad;
    HidMedia* hid_media;
    HidMusicMacos* hid_music_macos;
    HidMovie* hid_movie;
    HidMouse* hid_mouse;
    HidMouseClicker* hid_mouse_clicker;
    HidMouseJiggler* hid_mouse_jiggler;
    HidMouseJigglerStealth* hid_mouse_jiggler_stealth;
    HidTikTok* hid_tiktok;
    HidPushToTalk* hid_ptt;
    HidPushToTalkMenu* hid_ptt_menu;
    HidRemoteMenu*   hid_remote_menu;
    // Collections
    char    collection_names[BT_REMOTES_COLLECTION_MAX][BT_REMOTES_COLLECTION_NAME_LEN];
    uint8_t collection_count;
    char    pinned_collections[BT_REMOTES_PINNED_MAX][BT_REMOTES_COLLECTION_NAME_LEN];
    uint8_t pinned_kinds[BT_REMOTES_PINNED_MAX]; // 0 = collection, 1 = gesture
    uint8_t pinned_count;
    char    editing_collection_name[BT_REMOTES_COLLECTION_NAME_LEN];
    char    editing_collection_scripts[BT_REMOTES_COLLECTION_SCRIPT_MAX][256];
    uint8_t editing_collection_script_count;
    // Custom Gestures (global library + editing buffer)
    char    gesture_names[BT_REMOTES_GESTURE_MAX][BT_REMOTES_GESTURE_NAME_LEN];
    uint8_t gesture_count;
    char    editing_gesture_name[BT_REMOTES_GESTURE_NAME_LEN];
    // Profile management
    char active_profile[BT_REMOTES_PROFILE_NAME_LEN];
    char pending_name[BT_REMOTES_PROFILE_NAME_LEN]; // old name held during profile rename
    char default_ble_name[FURI_HAL_BT_ADV_NAME_LENGTH]; // default BT name applied to new profiles
    char profile_list[BT_REMOTES_PROFILE_MAX_COUNT][BT_REMOTES_PROFILE_NAME_LEN];
    uint8_t profile_count;
    // Per-profile remote-type settings
    uint8_t keynote_back_key; // KeynoteBackKey enum — which key short-press Back sends in Keynote
    uint8_t media_mode; // MediaMode enum — Legacy vs Improved media remote behavior
    uint8_t media_mouse_switch; // 0 = off, 1 = on — short Back opens mouse sub-view
    uint8_t tiktok_scroll_mode; // TikTokScrollMode enum — Wheel vs Gesture scrolling
    uint16_t tiktok_gesture_inset;  // px — horizontal inset before the vertical swipe
    uint16_t tiktok_gesture_margin; // px — vertical travel off the edge before press
    uint16_t tiktok_gesture_swipe;  // px — drag distance while the button is held
    uint8_t  delay_connect; // 0 = connect immediately; 1 = only connect inside a remote
    uint8_t  ducky_connect_per_run; // 1 = Ducky/Collections connect only during a script run
    uint16_t ducky_connect_settle_ms; // delay after link-up before sending HID (per-run)
    // App-level settings
    // 0=Neither, 1=Disconnect, 2=Connect, 3=Both
    uint8_t vibro_mode;
    // Persistent visual order for the Start menu.  0xFF = sentinel (unused slot).
    uint8_t  menu_order[BT_REMOTES_MENU_ORDER_LEN];
    uint32_t menu_hidden; // bitmask: bit i set → BtRemotesStartIndex i hidden in Start menu
    // Profile display order: profile names pipe-separated, loaded from app.cfg
    // profile_list[] is reordered to match this on every profile_load_list call
    char profile_order_str[BT_REMOTES_PROFILE_MAX_COUNT * (BT_REMOTES_PROFILE_NAME_LEN + 1)];
    // Ducky Scripts / file browser
    FileBrowser* file_browser;
    FuriString*  file_browser_result; // receives the selected file path
    DuckyRunner* ducky_runner;
    GestureRunner* gesture_runner; // runs Custom Gestures (separate engine)
    char pending_script_path[256]; // full path to the selected .txt script, copied from result
    // Post-pairing auto-save: polls for .bt_hid.keys after first-time BLE connect
    FuriTimer* pair_save_timer;
    uint8_t    pair_save_attempts;
    // Ducky "connect per run": polls app->connected while the run scene waits for the host
    FuriTimer* connect_wait_timer;
    uint8_t    connect_wait_attempts;
    uint8_t    connect_settle_ticks; // ticks counted since the link came up (HID-ready settle)
};

// Shared name validator: checks non-empty and no forbidden filesystem chars.
// Returns false and sets error on failure.  Collision check is caller's responsibility.
bool bt_remotes_validate_name(const char* text, FuriString* error);
// Shared popup helpers used by both run scenes (ducky + gesture).
const char* bt_remotes_path_basename(const char* path);
void        bt_remotes_show_running_popup(Hid* app);

// BLE lifecycle
void bt_remotes_start_ble(Hid* app);
void bt_remotes_start_ble_if_immediate(Hid* app); // start_ble unless delay_connect is set
void bt_remotes_stop_ble(Hid* app);
// Call from any Ducky/Collections browsing scene on_enter to enforce the
// connect-per-run "stay disconnected while browsing" policy in one place.
void bt_remotes_ducky_browse_enter(Hid* app);

// Config / pairing
void bt_hid_remove_pairing(Hid* app);
void bt_hid_save_cfg(Hid* app);
void bt_remotes_profile_clear_pairing(Hid* app);

// App-level config (default BT name, vibro mode, profile order)
void bt_remotes_load_app_cfg(Hid* app);
void bt_remotes_save_app_cfg(Hid* app);

// Write menu_order + menu_hidden into the active profile's .cfg (alongside name + mac).
// No-op if no profile is active.  Call whenever order or visibility changes.
void bt_remotes_save_profile_menu_cfg(Hid* app);

// Reorder profile_list[] to match profile_order_str (call after bt_remotes_profile_load_list)
void bt_remotes_apply_profile_order(Hid* app);

// Profile operations
void bt_remotes_profile_load_list(Hid* app);
bool bt_remotes_profile_create(Hid* app);
bool bt_remotes_profile_save(Hid* app);
bool bt_remotes_profile_activate(Hid* app);
bool bt_remotes_profile_delete(Hid* app);
bool bt_remotes_profile_rename(Hid* app);
bool bt_remotes_profile_reset(Hid* app);

// Collection operations
void bt_remotes_collection_load_list(Hid* app);
bool bt_remotes_collection_load(Hid* app, const char* name);
bool bt_remotes_collection_save(Hid* app);
bool bt_remotes_collection_delete(Hid* app, const char* name);
void bt_remotes_pinned_load(Hid* app);
void bt_remotes_pinned_save(Hid* app);

// Custom Gesture operations (global library; mirror the collection ops)
void bt_remotes_gesture_load_list(Hid* app);
// Full path to a gesture file by name (caller supplies buffer).
void bt_remotes_gesture_path(const char* name, char* out, size_t out_size);

// Default Start-menu item table — defined in bt_remotes_scene_start.c, shared with
// bt_remotes_scene_hide_items.c.  Entry [i].index == i always (table is in enum order).
extern const BtRemotesMenuEntry bt_remotes_menu_default[BT_REMOTES_MENU_ITEM_COUNT];

// HAL
void hid_hal_keyboard_press(Hid* instance, uint16_t event);
void hid_hal_keyboard_release(Hid* instance, uint16_t event);
void hid_hal_keyboard_release_all(Hid* instance);

void hid_hal_consumer_key_press(Hid* instance, uint16_t event);
void hid_hal_consumer_key_release(Hid* instance, uint16_t event);
void hid_hal_consumer_key_release_all(Hid* instance);

void hid_hal_mouse_move(Hid* instance, int8_t dx, int8_t dy);
void hid_hal_mouse_scroll(Hid* instance, int8_t delta);
void hid_hal_mouse_press(Hid* instance, uint16_t event);
void hid_hal_mouse_release(Hid* instance, uint16_t event);
void hid_hal_mouse_release_all(Hid* instance);
