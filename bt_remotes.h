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
#include <gui/modules/dialog_ex.h>
#include <gui/modules/popup.h>
#include <gui/modules/text_input.h>
#include "views/hid_remote_menu.h"
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
#define BT_REMOTES_MENU_ITEM_COUNT   14

#define BT_REMOTES_PROFILES_DIR APP_DATA_PATH("profiles")
#define BT_REMOTES_CFG_PATH     APP_DATA_PATH(".bt_hid.cfg")
#define BT_REMOTES_APP_CFG_PATH APP_DATA_PATH("app.cfg")
#define BT_REMOTES_CFG_EXT      ".cfg"
#define BT_REMOTES_KEYS_EXT     ".keys"

typedef struct Hid Hid;

struct Hid {
    FuriHalBleProfileBase* ble_hid_profile;
    BleProfileHidExtParams ble_hid_cfg;
    bool ble_started;
    Bt* bt;
    Gui* gui;
    NotificationApp* notifications;
    Storage* storage;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;
    Submenu* submenu;
    DialogEx* dialog;
    TextInput* text_input;
    Popup* popup;
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
    HidRemoteMenu* hid_remote_menu;
    // Profile management
    char active_profile[BT_REMOTES_PROFILE_NAME_LEN];
    char pending_name[BT_REMOTES_PROFILE_NAME_LEN]; // old name held during profile rename
    char default_ble_name[FURI_HAL_BT_ADV_NAME_LENGTH]; // default BT name applied to new profiles
    char profile_list[BT_REMOTES_PROFILE_MAX_COUNT][BT_REMOTES_PROFILE_NAME_LEN];
    uint8_t profile_count;
    // App-level settings
    // 0=Neither, 1=Disconnect, 2=Connect, 3=Both
    uint8_t vibro_mode;
    uint8_t  menu_order[BT_REMOTES_MENU_ITEM_COUNT]; // persistent visual order for Start menu
    uint16_t menu_hidden; // bitmask: bit i set → BtRemotesStartIndex i hidden in Start menu
    // Profile display order: profile names pipe-separated, loaded from app.cfg
    // profile_list[] is reordered to match this on every profile_load_list call
    char profile_order_str[BT_REMOTES_PROFILE_MAX_COUNT * (BT_REMOTES_PROFILE_NAME_LEN + 1)];
};

// BLE lifecycle
void bt_remotes_start_ble(Hid* app);
void bt_remotes_stop_ble(Hid* app);

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
