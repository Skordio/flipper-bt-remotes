#include "bt_remotes.h"
#include "views.h"
#include <flipper_format/flipper_format.h>
#include <furi_hal_random.h>
#include <notification/notification_messages.h>
#include <dolphin/dolphin.h>

#define TAG "BtRemotes"

#define PAIR_SAVE_POLL_MS      200
#define PAIR_SAVE_MAX_ATTEMPTS 25 // 5 seconds of polling

#define BT_REMOTES_CFG_FILE_TYPE     "Flipper BT Remote Settings File"
#define BT_REMOTES_CFG_VERSION       (1)
#define BT_REMOTES_APP_CFG_FILE_TYPE "Flipper BT Remotes App Config"
#define BT_REMOTES_APP_CFG_VERSION   (1)
#define BT_REMOTES_MAC_SIZE          (6)

// ---------------------------------------------------------------------------
// Config helpers
// ---------------------------------------------------------------------------

static void
    bt_remotes_write_cfg(Hid* app, const char* path, const char* name, uint8_t mac[BT_REMOTES_MAC_SIZE]) {
    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    if(flipper_format_file_open_always(fff, path)) {
        flipper_format_write_header_cstr(fff, BT_REMOTES_CFG_FILE_TYPE, BT_REMOTES_CFG_VERSION);
        flipper_format_write_string_cstr(fff, "name", name);
        flipper_format_write_hex(fff, "mac", mac, BT_REMOTES_MAC_SIZE);
        flipper_format_file_close(fff);
    }
    flipper_format_free(fff);
}

// ---------------------------------------------------------------------------
// App-level config (default BT name)
// ---------------------------------------------------------------------------

void bt_remotes_load_app_cfg(Hid* app) {
    app->vibro_mode = 1; // default: Disconnect
    // menu_order and menu_hidden are per-profile — loaded by bt_remotes_profile_activate.
    // Initialise to safe defaults here so the struct is never uninitialised.
    for(uint8_t i = 0; i < BT_REMOTES_MENU_ITEM_COUNT; i++) app->menu_order[i] = i;
    for(uint8_t i = BT_REMOTES_MENU_ITEM_COUNT; i < BT_REMOTES_MENU_ORDER_LEN; i++)
        app->menu_order[i] = 0xFF; // sentinel: custom-remote slot not yet placed
    app->menu_hidden = 0;
    app->profile_order_str[0] = '\0'; // empty = no saved profile order

    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    FuriString* tmp = furi_string_alloc();
    uint32_t ver = 0;
    do {
        if(!flipper_format_file_open_existing(fff, BT_REMOTES_APP_CFG_PATH)) break;
        if(!flipper_format_read_header(fff, tmp, &ver)) break;
        if(strcmp(furi_string_get_cstr(tmp), BT_REMOTES_APP_CFG_FILE_TYPE) != 0 ||
           ver != BT_REMOTES_APP_CFG_VERSION)
            break;
        if(flipper_format_read_string(fff, "default_name", tmp)) {
            strlcpy(
                app->default_ble_name,
                furi_string_get_cstr(tmp),
                sizeof(app->default_ble_name));
        } else {
            flipper_format_rewind(fff);
        }
        // vibro_mode: 0=Neither, 1=Disconnect, 2=Connect, 3=Both
        // Old cfg files had "disconnect_vibro" bool — those won't parse as uint32,
        // so we default to 1 (Disconnect) when the key is absent.
        uint32_t vibro_mode_u32 = 1;
        if(flipper_format_read_uint32(fff, "vibro_mode", &vibro_mode_u32, 1)) {
            if(vibro_mode_u32 > 3) vibro_mode_u32 = 1;
            app->vibro_mode = (uint8_t)vibro_mode_u32;
        }
        // Profile order: pipe-separated profile names
        flipper_format_rewind(fff);
        if(flipper_format_read_string(fff, "profile_order", tmp)) {
            strlcpy(
                app->profile_order_str,
                furi_string_get_cstr(tmp),
                sizeof(app->profile_order_str));
        }
    } while(0);
    furi_string_free(tmp);
    flipper_format_free(fff);
}

void bt_remotes_save_app_cfg(Hid* app) {
    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    if(flipper_format_file_open_always(fff, BT_REMOTES_APP_CFG_PATH)) {
        flipper_format_write_header_cstr(
            fff, BT_REMOTES_APP_CFG_FILE_TYPE, BT_REMOTES_APP_CFG_VERSION);
        flipper_format_write_string_cstr(fff, "default_name", app->default_ble_name);
        uint32_t vibro_mode_u32 = app->vibro_mode;
        flipper_format_write_uint32(fff, "vibro_mode", &vibro_mode_u32, 1);
        flipper_format_write_string_cstr(fff, "profile_order", app->profile_order_str);
        flipper_format_file_close(fff);
    }
    flipper_format_free(fff);
}

static void bt_remotes_load_cfg(Hid* app) {
    memset(&app->ble_hid_cfg, 0, sizeof(app->ble_hid_cfg));

    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    FuriString* tmp = furi_string_alloc();
    uint32_t ver = 0;

    do {
        if(!flipper_format_file_open_existing(fff, BT_REMOTES_CFG_PATH)) break;
        if(!flipper_format_read_header(fff, tmp, &ver)) break;
        if(strcmp(furi_string_get_cstr(tmp), BT_REMOTES_CFG_FILE_TYPE) != 0 ||
           ver != BT_REMOTES_CFG_VERSION)
            break;
        if(flipper_format_read_string(fff, "name", tmp)) {
            strlcpy(
                app->ble_hid_cfg.name,
                furi_string_get_cstr(tmp),
                sizeof(app->ble_hid_cfg.name));
        } else {
            flipper_format_rewind(fff);
        }
        flipper_format_read_hex(fff, "mac", app->ble_hid_cfg.mac, sizeof(app->ble_hid_cfg.mac));
    } while(0);

    furi_string_free(tmp);
    flipper_format_free(fff);
}

void bt_hid_save_cfg(Hid* app) {
    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    if(flipper_format_file_open_always(fff, BT_REMOTES_CFG_PATH)) {
        flipper_format_write_header_cstr(fff, BT_REMOTES_CFG_FILE_TYPE, BT_REMOTES_CFG_VERSION);
        flipper_format_write_string_cstr(fff, "name", app->ble_hid_cfg.name);
        flipper_format_write_hex(fff, "mac", app->ble_hid_cfg.mac, BT_REMOTES_MAC_SIZE);
        flipper_format_file_close(fff);
    }
    flipper_format_free(fff);
}

// Write the complete profile .cfg (name + mac + menu_order + menu_hidden) for the active profile.
// Also keeps .bt_hid.cfg in sync so bt_remotes_profile_save's key snapshot uses the right base.
// No-op when no profile is active.
void bt_remotes_save_profile_menu_cfg(Hid* app) {
    if(app->active_profile[0] == '\0') return;

    FuriString* path = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->active_profile, BT_REMOTES_CFG_EXT);

    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    if(flipper_format_file_open_always(fff, furi_string_get_cstr(path))) {
        flipper_format_write_header_cstr(fff, BT_REMOTES_CFG_FILE_TYPE, BT_REMOTES_CFG_VERSION);
        flipper_format_write_string_cstr(fff, "name", app->ble_hid_cfg.name);
        flipper_format_write_hex(fff, "mac", app->ble_hid_cfg.mac, BT_REMOTES_MAC_SIZE);
        uint32_t order_u32[BT_REMOTES_MENU_ORDER_LEN];
        for(uint8_t i = 0; i < BT_REMOTES_MENU_ORDER_LEN; i++) order_u32[i] = app->menu_order[i];
        flipper_format_write_uint32(fff, "menu_order", order_u32, BT_REMOTES_MENU_ORDER_LEN);
        uint32_t hidden_u32 = app->menu_hidden;
        flipper_format_write_uint32(fff, "menu_hidden", &hidden_u32, 1);
        uint32_t keynote_back_key_u32 = app->keynote_back_key;
        flipper_format_write_uint32(fff, "keynote_back_key", &keynote_back_key_u32, 1);
        uint32_t media_mode_u32 = app->media_mode;
        flipper_format_write_uint32(fff, "media_mode", &media_mode_u32, 1);
        uint32_t media_mouse_switch_u32 = app->media_mouse_switch;
        flipper_format_write_uint32(fff, "media_mouse_switch", &media_mouse_switch_u32, 1);
        uint32_t tiktok_scroll_mode_u32 = app->tiktok_scroll_mode;
        flipper_format_write_uint32(fff, "tiktok_scroll_mode", &tiktok_scroll_mode_u32, 1);
        uint32_t tiktok_gesture_inset_u32 = app->tiktok_gesture_inset;
        flipper_format_write_uint32(fff, "tiktok_gesture_inset", &tiktok_gesture_inset_u32, 1);
        uint32_t tiktok_gesture_margin_u32 = app->tiktok_gesture_margin;
        flipper_format_write_uint32(fff, "tiktok_gesture_margin", &tiktok_gesture_margin_u32, 1);
        uint32_t tiktok_gesture_swipe_u32 = app->tiktok_gesture_swipe;
        flipper_format_write_uint32(fff, "tiktok_gesture_swipe", &tiktok_gesture_swipe_u32, 1);
        uint32_t delay_connect_u32 = app->delay_connect;
        flipper_format_write_uint32(fff, "delay_connect", &delay_connect_u32, 1);
        uint32_t ducky_connect_per_run_u32 = app->ducky_connect_per_run;
        flipper_format_write_uint32(fff, "ducky_connect_per_run", &ducky_connect_per_run_u32, 1);
        uint32_t ducky_connect_settle_ms_u32 = app->ducky_connect_settle_ms;
        flipper_format_write_uint32(fff, "ducky_connect_settle_ms", &ducky_connect_settle_ms_u32, 1);
        flipper_format_file_close(fff);
    }
    flipper_format_free(fff);
    furi_string_free(path);
    FURI_LOG_D(TAG, "Profile menu cfg saved: %s", app->active_profile);
}

// ---------------------------------------------------------------------------
// Pairing helpers
// ---------------------------------------------------------------------------

void bt_hid_remove_pairing(Hid* app) {
    bt_disconnect(app->bt);
    furi_delay_ms(200);
    furi_hal_bt_stop_advertising();
    bt_forget_bonded_devices(app->bt);
    furi_hal_bt_start_advertising();
}

// Clear pairing keys from disk without touching the BLE stack.
// Safe to call when BLE is stopped (e.g. from inside the Settings scene).
// After calling this, the next BLE start will advertise without bonding data
// so the host must pair again.
void bt_remotes_profile_clear_pairing(Hid* app) {
    storage_common_remove(app->storage, APP_DATA_PATH(HID_BT_KEYS_STORAGE_NAME));
    if(app->active_profile[0] != '\0') {
        FuriString* prof_keys = furi_string_alloc_printf(
            "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->active_profile, BT_REMOTES_KEYS_EXT);
        storage_common_remove(app->storage, furi_string_get_cstr(prof_keys));
        furi_string_free(prof_keys);
        FURI_LOG_I(TAG, "Pairing cleared for profile: %s", app->active_profile);
    }
}

// ---------------------------------------------------------------------------
// Profile operations
// ---------------------------------------------------------------------------

void bt_remotes_profile_load_list(Hid* app) {
    app->profile_count = 0;
    File* dir = storage_file_alloc(app->storage);
    FileInfo info;
    char name[BT_REMOTES_PROFILE_NAME_LEN + 8];

    if(storage_dir_open(dir, BT_REMOTES_PROFILES_DIR)) {
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            if(name[0] == '.') continue;
            if(app->profile_count >= BT_REMOTES_PROFILE_MAX_COUNT) break;

            size_t len = strlen(name);
            size_t ext_len = strlen(BT_REMOTES_CFG_EXT);
            if(len <= ext_len) continue;
            if(strcmp(name + len - ext_len, BT_REMOTES_CFG_EXT) != 0) continue;

            name[len - ext_len] = '\0';
            strlcpy(
                app->profile_list[app->profile_count],
                name,
                BT_REMOTES_PROFILE_NAME_LEN);
            app->profile_count++;
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);

    bt_remotes_apply_profile_order(app);
}

// Reorder profile_list[] to match the saved pipe-separated profile_order_str.
// Profiles not in the saved order are appended at the end (new profiles since last save).
// Profiles in the saved order that no longer exist on disk are silently dropped.
void bt_remotes_apply_profile_order(Hid* app) {
    if(app->profile_order_str[0] == '\0' || app->profile_count == 0) return;

    // Parse the saved order into a temporary name list
    char ordered[BT_REMOTES_PROFILE_MAX_COUNT][BT_REMOTES_PROFILE_NAME_LEN];
    uint8_t ordered_count = 0;
    bool in_list[BT_REMOTES_PROFILE_MAX_COUNT];
    memset(in_list, 0, sizeof(in_list));

    // Work on a copy so strtok doesn't corrupt the struct field
    char tmp[BT_REMOTES_PROFILE_MAX_COUNT * (BT_REMOTES_PROFILE_NAME_LEN + 1)];
    strlcpy(tmp, app->profile_order_str, sizeof(tmp));

    char* token = strtok(tmp, "|");
    while(token && ordered_count < BT_REMOTES_PROFILE_MAX_COUNT) {
        // Find this name in the current (disk-loaded) profile_list
        for(uint8_t i = 0; i < app->profile_count; i++) {
            if(!in_list[i] && strcmp(app->profile_list[i], token) == 0) {
                strlcpy(
                    ordered[ordered_count++], app->profile_list[i], BT_REMOTES_PROFILE_NAME_LEN);
                in_list[i] = true;
                break;
            }
        }
        token = strtok(NULL, "|");
    }

    // Append any profiles not present in the saved order (newly created profiles)
    for(uint8_t i = 0; i < app->profile_count && ordered_count < BT_REMOTES_PROFILE_MAX_COUNT;
        i++) {
        if(!in_list[i]) {
            strlcpy(
                ordered[ordered_count++], app->profile_list[i], BT_REMOTES_PROFILE_NAME_LEN);
        }
    }

    // Write the sorted order back into profile_list
    for(uint8_t i = 0; i < ordered_count; i++) {
        strlcpy(app->profile_list[i], ordered[i], BT_REMOTES_PROFILE_NAME_LEN);
    }
    app->profile_count = ordered_count;
    FURI_LOG_I(TAG, "Profile order applied: %u profiles", ordered_count);
}

bool bt_remotes_profile_create(Hid* app) {
    storage_simply_mkdir(app->storage, BT_REMOTES_PROFILES_DIR);

    // Generate static-random BLE public address (bits 7:6 of byte[5] = 11)
    uint8_t mac[BT_REMOTES_MAC_SIZE];
    furi_hal_random_fill_buf(mac, BT_REMOTES_MAC_SIZE);
    mac[5] |= 0xC0;

    // Write profile cfg — inherit the current default BT name
    FuriString* dst = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->active_profile, BT_REMOTES_CFG_EXT);
    bool ok = false;
    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    if(flipper_format_file_open_always(fff, furi_string_get_cstr(dst))) {
        ok = flipper_format_write_header_cstr(
                 fff, BT_REMOTES_CFG_FILE_TYPE, BT_REMOTES_CFG_VERSION) &&
             flipper_format_write_string_cstr(fff, "name", app->default_ble_name) &&
             flipper_format_write_hex(fff, "mac", mac, BT_REMOTES_MAC_SIZE);
        flipper_format_file_close(fff);
    }
    flipper_format_free(fff);
    furi_string_free(dst);

    if(ok) {
        // Push the new MAC + default name into the active cfg so BLE starts with it
        bt_remotes_write_cfg(app, BT_REMOTES_CFG_PATH, app->default_ble_name, mac);
        // Remove stale bonding data — new device will pair fresh
        storage_common_remove(app->storage, APP_DATA_PATH(HID_BT_KEYS_STORAGE_NAME));
        FURI_LOG_I(TAG, "Profile created: %s", app->active_profile);
    }

    return ok;
}

bool bt_remotes_profile_save(Hid* app) {
    const char* keys_path = APP_DATA_PATH(HID_BT_KEYS_STORAGE_NAME);
    if(!storage_file_exists(app->storage, keys_path)) {
        FURI_LOG_W(TAG, "No pairing keys to save");
        return false;
    }

    storage_simply_mkdir(app->storage, BT_REMOTES_PROFILES_DIR);

    FuriString* dst = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->active_profile, BT_REMOTES_KEYS_EXT);

    storage_common_remove(app->storage, furi_string_get_cstr(dst));
    FS_Error err =
        storage_common_copy(app->storage, keys_path, furi_string_get_cstr(dst));

    furi_string_free(dst);

    if(err != FSE_OK) {
        FURI_LOG_E(TAG, "Profile save (keys) failed: %d", err);
        return false;
    }

    // Snapshot the full profile cfg (name + mac + menu_order + menu_hidden) so that
    // activating the profile later restores all settings, not just the BLE identity.
    bt_remotes_save_profile_menu_cfg(app);

    FURI_LOG_I(TAG, "Profile saved: %s", app->active_profile);
    return true;
}

bool bt_remotes_profile_activate(Hid* app) {
    FuriString* src_cfg = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->active_profile, BT_REMOTES_CFG_EXT);
    FuriString* src_keys = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->active_profile, BT_REMOTES_KEYS_EXT);

    // Restore cfg (contains the profile's MAC)
    if(!storage_file_exists(app->storage, furi_string_get_cstr(src_cfg))) {
        FURI_LOG_E(TAG, "Profile cfg missing: %s", app->active_profile);
        furi_string_free(src_cfg);
        furi_string_free(src_keys);
        return false;
    }
    storage_common_remove(app->storage, BT_REMOTES_CFG_PATH);
    storage_common_copy(app->storage, furi_string_get_cstr(src_cfg), BT_REMOTES_CFG_PATH);

    // Restore bonding keys
    const char* keys_path = APP_DATA_PATH(HID_BT_KEYS_STORAGE_NAME);
    bool ok = false;
    if(storage_file_exists(app->storage, furi_string_get_cstr(src_keys))) {
        storage_common_remove(app->storage, keys_path);
        FS_Error err =
            storage_common_copy(app->storage, furi_string_get_cstr(src_keys), keys_path);
        ok = (err == FSE_OK);
        if(!ok) {
            FURI_LOG_E(TAG, "Keys restore failed: %d", err);
        }
    } else {
        // MAC-only profile: clear stale bonding data so host must pair fresh
        storage_common_remove(app->storage, keys_path);
        ok = true;
    }

    // Load per-profile menu settings (menu_order, menu_hidden).
    // These live in the profile .cfg — missing keys in old-format files get safe defaults.
    {
        // Safe defaults: fixed items in natural order, custom-remote slots empty.
        for(uint8_t i = 0; i < BT_REMOTES_MENU_ITEM_COUNT; i++) app->menu_order[i] = i;
        for(uint8_t i = BT_REMOTES_MENU_ITEM_COUNT; i < BT_REMOTES_MENU_ORDER_LEN; i++)
            app->menu_order[i] = 0xFF;
        app->menu_hidden = 0;

        FlipperFormat* mfff = flipper_format_file_alloc(app->storage);
        FuriString*    mtmp = furi_string_alloc();
        uint32_t       mver = 0;
        do {
            if(!flipper_format_file_open_existing(mfff, furi_string_get_cstr(src_cfg))) break;
            if(!flipper_format_read_header(mfff, mtmp, &mver)) break;

            // Try the new 32-entry format first; fall back to the old 16-entry format.
            uint32_t order_u32[BT_REMOTES_MENU_ORDER_LEN];
            for(uint8_t i = 0; i < BT_REMOTES_MENU_ORDER_LEN; i++) order_u32[i] = 0xFF;
            if(flipper_format_read_uint32(
                   mfff, "menu_order", order_u32, BT_REMOTES_MENU_ORDER_LEN)) {
                // New format: validate and copy all 32 slots.
                for(uint8_t i = 0; i < BT_REMOTES_MENU_ORDER_LEN; i++) {
                    uint8_t v = (uint8_t)order_u32[i];
                    // Valid values: fixed indices 0‥15, custom-remote indices 16‥31, 0xFF sentinel.
                    app->menu_order[i] = (v < BT_REMOTES_MENU_ORDER_LEN || v == 0xFF) ? v : 0xFF;
                }
            } else {
                // Previous format (31 entries: 15 fixed + 16 pinned), saved before
                // Custom Gestures existed. Adding a fixed item moved the
                // fixed/pinned boundary 15 -> 16, so remap old pinned-slot values
                // (15..30) by +1. Old fixed index 14 (Settings) is left as 14,
                // which now denotes Custom Gestures; the real Settings (15) is
                // re-appended by start_on_enter, landing Custom Gestures just above
                // Settings for default layouts.
                flipper_format_rewind(mfff);
                for(uint8_t i = 0; i < BT_REMOTES_MENU_ORDER_LEN; i++) order_u32[i] = 0xFF;
                if(flipper_format_read_uint32(
                       mfff, "menu_order", order_u32, BT_REMOTES_MENU_ORDER_LEN_V1)) {
                    for(uint8_t i = 0; i < BT_REMOTES_MENU_ORDER_LEN_V1; i++) {
                        uint8_t v = (uint8_t)order_u32[i];
                        if(v == 0xFF) {
                            app->menu_order[i] = 0xFF;
                        } else if(
                            v >= BT_REMOTES_MENU_ITEM_COUNT_V1 &&
                            v < BT_REMOTES_MENU_ORDER_LEN_V1) {
                            app->menu_order[i] = (uint8_t)(v + 1); // pinned slot shifted +1
                        } else if(v < BT_REMOTES_MENU_ITEM_COUNT_V1) {
                            app->menu_order[i] = v; // fixed item index unchanged
                        } else {
                            app->menu_order[i] = 0xFF;
                        }
                    }
                    // New trailing slot (index 31) stays 0xFF.
                } else {
                    // Even older format (just the 15 fixed items): migrate; pinned
                    // slots default to 0xFF.
                    flipper_format_rewind(mfff);
                    for(uint8_t i = 0; i < BT_REMOTES_MENU_ORDER_LEN; i++) order_u32[i] = 0xFF;
                    if(flipper_format_read_uint32(
                           mfff, "menu_order", order_u32, BT_REMOTES_MENU_ITEM_COUNT_V1)) {
                        for(uint8_t i = 0; i < BT_REMOTES_MENU_ITEM_COUNT_V1; i++) {
                            app->menu_order[i] = (uint8_t)(
                                order_u32[i] < BT_REMOTES_MENU_ITEM_COUNT_V1 ? order_u32[i] : i);
                        }
                    }
                }
            }
            flipper_format_rewind(mfff);
            uint32_t hidden_u32 = 0;
            if(flipper_format_read_uint32(mfff, "menu_hidden", &hidden_u32, 1)) {
                app->menu_hidden = hidden_u32;
                // Settings must never be hidden (it is the always-last fixed item,
                // and the only item hide_items keeps out of its toggle list).
                app->menu_hidden &= ~(uint32_t)(1u << BtRemotesStartIndexSettings);
            }
            flipper_format_rewind(mfff);
            uint32_t keynote_back_key_u32 = KEYNOTE_BACK_KEY_DEFAULT;
            if(flipper_format_read_uint32(mfff, "keynote_back_key", &keynote_back_key_u32, 1)) {
                if(keynote_back_key_u32 >= KEYNOTE_BACK_KEY_COUNT)
                    keynote_back_key_u32 = KEYNOTE_BACK_KEY_DEFAULT;
                app->keynote_back_key = (uint8_t)keynote_back_key_u32;
            } else {
                app->keynote_back_key = KEYNOTE_BACK_KEY_DEFAULT;
            }
            flipper_format_rewind(mfff);
            uint32_t media_mode_u32 = MEDIA_MODE_DEFAULT;
            if(flipper_format_read_uint32(mfff, "media_mode", &media_mode_u32, 1)) {
                if(media_mode_u32 >= MEDIA_MODE_COUNT) media_mode_u32 = MEDIA_MODE_DEFAULT;
                app->media_mode = (uint8_t)media_mode_u32;
            } else {
                app->media_mode = MEDIA_MODE_DEFAULT;
            }
            flipper_format_rewind(mfff);
            uint32_t media_mouse_switch_u32 = MEDIA_MOUSE_SWITCH_DEFAULT;
            if(flipper_format_read_uint32(mfff, "media_mouse_switch", &media_mouse_switch_u32, 1)) {
                app->media_mouse_switch = media_mouse_switch_u32 ? 1 : 0;
            } else {
                app->media_mouse_switch = MEDIA_MOUSE_SWITCH_DEFAULT;
            }
            flipper_format_rewind(mfff);
            uint32_t tiktok_scroll_mode_u32 = TIKTOK_SCROLL_MODE_DEFAULT;
            if(flipper_format_read_uint32(mfff, "tiktok_scroll_mode", &tiktok_scroll_mode_u32, 1)) {
                if(tiktok_scroll_mode_u32 >= TIKTOK_SCROLL_MODE_COUNT)
                    tiktok_scroll_mode_u32 = TIKTOK_SCROLL_MODE_DEFAULT;
                app->tiktok_scroll_mode = (uint8_t)tiktok_scroll_mode_u32;
            } else {
                app->tiktok_scroll_mode = TIKTOK_SCROLL_MODE_DEFAULT;
            }
            flipper_format_rewind(mfff);
            uint32_t tiktok_gesture_inset_u32 = TIKTOK_GESTURE_INSET_DEFAULT;
            if(flipper_format_read_uint32(
                   mfff, "tiktok_gesture_inset", &tiktok_gesture_inset_u32, 1)) {
                if(tiktok_gesture_inset_u32 < TIKTOK_GESTURE_INSET_MIN)
                    tiktok_gesture_inset_u32 = TIKTOK_GESTURE_INSET_MIN;
                if(tiktok_gesture_inset_u32 > TIKTOK_GESTURE_INSET_MAX)
                    tiktok_gesture_inset_u32 = TIKTOK_GESTURE_INSET_MAX;
                app->tiktok_gesture_inset = (uint16_t)tiktok_gesture_inset_u32;
            } else {
                app->tiktok_gesture_inset = TIKTOK_GESTURE_INSET_DEFAULT;
            }
            flipper_format_rewind(mfff);
            uint32_t tiktok_gesture_margin_u32 = TIKTOK_GESTURE_MARGIN_DEFAULT;
            if(flipper_format_read_uint32(
                   mfff, "tiktok_gesture_margin", &tiktok_gesture_margin_u32, 1)) {
                if(tiktok_gesture_margin_u32 < TIKTOK_GESTURE_MARGIN_MIN)
                    tiktok_gesture_margin_u32 = TIKTOK_GESTURE_MARGIN_MIN;
                if(tiktok_gesture_margin_u32 > TIKTOK_GESTURE_MARGIN_MAX)
                    tiktok_gesture_margin_u32 = TIKTOK_GESTURE_MARGIN_MAX;
                app->tiktok_gesture_margin = (uint16_t)tiktok_gesture_margin_u32;
            } else {
                app->tiktok_gesture_margin = TIKTOK_GESTURE_MARGIN_DEFAULT;
            }
            flipper_format_rewind(mfff);
            uint32_t tiktok_gesture_swipe_u32 = TIKTOK_GESTURE_SWIPE_DEFAULT;
            if(flipper_format_read_uint32(
                   mfff, "tiktok_gesture_swipe", &tiktok_gesture_swipe_u32, 1)) {
                if(tiktok_gesture_swipe_u32 < TIKTOK_GESTURE_SWIPE_MIN)
                    tiktok_gesture_swipe_u32 = TIKTOK_GESTURE_SWIPE_MIN;
                if(tiktok_gesture_swipe_u32 > TIKTOK_GESTURE_SWIPE_MAX)
                    tiktok_gesture_swipe_u32 = TIKTOK_GESTURE_SWIPE_MAX;
                app->tiktok_gesture_swipe = (uint16_t)tiktok_gesture_swipe_u32;
            } else {
                app->tiktok_gesture_swipe = TIKTOK_GESTURE_SWIPE_DEFAULT;
            }
            // The three fields below appear consecutively at the end of the save
            // order, so we need only one rewind to reset past the tiktok reads.
            flipper_format_rewind(mfff);
            uint32_t delay_connect_u32 = DELAY_CONNECT_DEFAULT;
            if(flipper_format_read_uint32(mfff, "delay_connect", &delay_connect_u32, 1)) {
                app->delay_connect = delay_connect_u32 ? 1 : 0;
            } else {
                app->delay_connect = DELAY_CONNECT_DEFAULT;
            }
            uint32_t ducky_connect_per_run_u32 = DUCKY_CONNECT_PER_RUN_DEFAULT;
            if(flipper_format_read_uint32(
                   mfff, "ducky_connect_per_run", &ducky_connect_per_run_u32, 1)) {
                app->ducky_connect_per_run = ducky_connect_per_run_u32 ? 1 : 0;
            } else {
                app->ducky_connect_per_run = DUCKY_CONNECT_PER_RUN_DEFAULT;
            }
            uint32_t ducky_connect_settle_ms_u32 = DUCKY_CONNECT_SETTLE_DEFAULT;
            if(flipper_format_read_uint32(
                   mfff, "ducky_connect_settle_ms", &ducky_connect_settle_ms_u32, 1)) {
                if(ducky_connect_settle_ms_u32 > DUCKY_CONNECT_SETTLE_MAX)
                    ducky_connect_settle_ms_u32 = DUCKY_CONNECT_SETTLE_MAX;
                // Snap to a whole step so the run-scene tick math is exact.
                ducky_connect_settle_ms_u32 =
                    (ducky_connect_settle_ms_u32 / DUCKY_CONNECT_SETTLE_STEP) *
                    DUCKY_CONNECT_SETTLE_STEP;
                app->ducky_connect_settle_ms = (uint16_t)ducky_connect_settle_ms_u32;
            } else {
                app->ducky_connect_settle_ms = DUCKY_CONNECT_SETTLE_DEFAULT;
            }
        } while(0);
        furi_string_free(mtmp);
        flipper_format_free(mfff);
    }

    furi_string_free(src_cfg);
    furi_string_free(src_keys);

    if(ok) {
        FURI_LOG_I(TAG, "Profile activated: %s", app->active_profile);
    }
    return ok;
}

bool bt_remotes_profile_delete(Hid* app) {
    FuriString* cfg = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->active_profile, BT_REMOTES_CFG_EXT);
    FuriString* keys = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->active_profile, BT_REMOTES_KEYS_EXT);

    FS_Error e1 = storage_common_remove(app->storage, furi_string_get_cstr(cfg));
    FS_Error e2 = storage_common_remove(app->storage, furi_string_get_cstr(keys));

    furi_string_free(cfg);
    furi_string_free(keys);

    return (e1 == FSE_OK || e1 == FSE_NOT_EXIST) &&
           (e2 == FSE_OK || e2 == FSE_NOT_EXIST);
}

bool bt_remotes_profile_rename(Hid* app) {
    // pending_name = old filename, active_profile = new filename (written by text_input)
    FuriString* old_cfg = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->pending_name, BT_REMOTES_CFG_EXT);
    FuriString* old_keys = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->pending_name, BT_REMOTES_KEYS_EXT);
    FuriString* new_cfg = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->active_profile, BT_REMOTES_CFG_EXT);
    FuriString* new_keys = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->active_profile, BT_REMOTES_KEYS_EXT);

    FS_Error err = storage_common_copy(
        app->storage, furi_string_get_cstr(old_cfg), furi_string_get_cstr(new_cfg));
    bool ok = (err == FSE_OK);

    if(ok) {
        storage_common_remove(app->storage, furi_string_get_cstr(old_cfg));

        if(storage_file_exists(app->storage, furi_string_get_cstr(old_keys))) {
            err = storage_common_copy(
                app->storage,
                furi_string_get_cstr(old_keys),
                furi_string_get_cstr(new_keys));
            if(err == FSE_OK) {
                storage_common_remove(app->storage, furi_string_get_cstr(old_keys));
            } else {
                // Keys copy failed — treat rename as failed so the scene doesn't
                // report success with pairing data silently lost.
                FURI_LOG_E(TAG, "Profile rename: keys copy failed (%d)", err);
                ok = false;
            }
        }

        if(ok) {
            // Update the in-memory profile order string so apply_profile_order
            // finds the new name on the next launch instead of appending it at
            // the end (losing the saved position in the list).
            char* found = strstr(app->profile_order_str, app->pending_name);
            if(found) {
                size_t old_len = strlen(app->pending_name);
                size_t new_len = strlen(app->active_profile);
                size_t rest    = strlen(found + old_len);
                memmove(found + new_len, found + old_len, rest + 1);
                memcpy(found, app->active_profile, new_len);
            }
            bt_remotes_save_app_cfg(app);
            FURI_LOG_I(TAG, "Profile renamed: %s → %s", app->pending_name, app->active_profile);
        }
    } else {
        FURI_LOG_E(TAG, "Profile rename failed: %d", err);
    }

    furi_string_free(old_cfg);
    furi_string_free(old_keys);
    furi_string_free(new_cfg);
    furi_string_free(new_keys);
    return ok;
}

bool bt_remotes_profile_reset(Hid* app) {
    uint8_t mac[BT_REMOTES_MAC_SIZE];
    furi_hal_random_fill_buf(mac, BT_REMOTES_MAC_SIZE);
    mac[5] |= 0xC0; // static-random address format

    memcpy(app->ble_hid_cfg.mac, mac, BT_REMOTES_MAC_SIZE);
    // name is intentionally preserved

    bt_hid_save_cfg(app); // writes new MAC + existing name to .bt_hid.cfg

    // Snapshot updated cfg (including menu_order + menu_hidden) into the profile directory
    bt_remotes_save_profile_menu_cfg(app);

    // Wipe all bonding data so host must pair fresh
    FuriString* prof_keys = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->active_profile, BT_REMOTES_KEYS_EXT);
    storage_common_remove(app->storage, furi_string_get_cstr(prof_keys));
    furi_string_free(prof_keys);
    storage_common_remove(app->storage, APP_DATA_PATH(HID_BT_KEYS_STORAGE_NAME));

    FURI_LOG_I(TAG, "Profile reset: %s", app->active_profile);
    return true;
}

// ---------------------------------------------------------------------------
// Collection operations
// ---------------------------------------------------------------------------

#define BT_REMOTES_COLLECTION_FILE_TYPE    "Flipper BT Ducky Collection"
#define BT_REMOTES_COLLECTION_FILE_VERSION (1)
#define BT_REMOTES_PINS_FILE_TYPE          "Flipper BT Collection Pins"
#define BT_REMOTES_PINS_FILE_VERSION       (1)

void bt_remotes_collection_load_list(Hid* app) {
    app->collection_count = 0;

    File*    dir = storage_file_alloc(app->storage);
    FileInfo info;
    char     name[BT_REMOTES_COLLECTION_NAME_LEN + 16];

    if(storage_dir_open(dir, BT_REMOTES_COLLECTION_DIR)) {
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            if(name[0] == '.') continue;
            if(app->collection_count >= BT_REMOTES_COLLECTION_MAX) break;
            size_t len     = strlen(name);
            size_t ext_len = strlen(BT_REMOTES_COLLECTION_EXT);
            if(len <= ext_len) continue;
            if(strcmp(name + len - ext_len, BT_REMOTES_COLLECTION_EXT) != 0) continue;
            name[len - ext_len] = '\0';
            strlcpy(
                app->collection_names[app->collection_count],
                name,
                BT_REMOTES_COLLECTION_NAME_LEN);
            app->collection_count++;
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
}

bool bt_remotes_collection_load(Hid* app, const char* name) {
    strlcpy(app->editing_collection_name, name, BT_REMOTES_COLLECTION_NAME_LEN);
    app->editing_collection_script_count = 0;

    FuriString* path = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_COLLECTION_DIR, name, BT_REMOTES_COLLECTION_EXT);

    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    FuriString*    tmp = furi_string_alloc();
    uint32_t       ver = 0;
    bool           ok  = false;

    do {
        if(!flipper_format_file_open_existing(fff, furi_string_get_cstr(path))) break;
        if(!flipper_format_read_header(fff, tmp, &ver)) break;
        if(strcmp(furi_string_get_cstr(tmp), BT_REMOTES_COLLECTION_FILE_TYPE) != 0) break;

        uint32_t count = 0;
        if(!flipper_format_read_uint32(fff, "count", &count, 1)) break;
        ok = true;
        if(count > BT_REMOTES_COLLECTION_SCRIPT_MAX) count = BT_REMOTES_COLLECTION_SCRIPT_MAX;

        for(uint32_t i = 0; i < count; i++) {
            char key[20];
            snprintf(key, sizeof(key), "script_%u", (unsigned)i);
            if(flipper_format_read_string(fff, key, tmp)) {
                strlcpy(
                    app->editing_collection_scripts[app->editing_collection_script_count],
                    furi_string_get_cstr(tmp),
                    256);
                app->editing_collection_script_count++;
            }
        }
    } while(0);

    furi_string_free(tmp);
    flipper_format_free(fff);
    furi_string_free(path);
    return ok;
}

bool bt_remotes_collection_save(Hid* app) {
    storage_simply_mkdir(app->storage, BT_REMOTES_COLLECTION_DIR);

    FuriString* path = furi_string_alloc_printf(
        "%s/%s%s",
        BT_REMOTES_COLLECTION_DIR,
        app->editing_collection_name,
        BT_REMOTES_COLLECTION_EXT);

    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    bool           ok  = false;

    if(flipper_format_file_open_always(fff, furi_string_get_cstr(path))) {
        ok = flipper_format_write_header_cstr(
            fff, BT_REMOTES_COLLECTION_FILE_TYPE, BT_REMOTES_COLLECTION_FILE_VERSION);
        uint32_t count = app->editing_collection_script_count;
        ok = ok && flipper_format_write_uint32(fff, "count", &count, 1);
        for(uint8_t i = 0; ok && i < app->editing_collection_script_count; i++) {
            char key[20];
            snprintf(key, sizeof(key), "script_%u", (unsigned)i);
            ok = flipper_format_write_string_cstr(
                fff, key, app->editing_collection_scripts[i]);
        }
        flipper_format_file_close(fff);
    }
    flipper_format_free(fff);
    furi_string_free(path);
    return ok;
}

bool bt_remotes_collection_delete(Hid* app, const char* name) {
    FuriString* path = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_COLLECTION_DIR, name, BT_REMOTES_COLLECTION_EXT);
    FS_Error err = storage_common_remove(app->storage, furi_string_get_cstr(path));
    furi_string_free(path);

    // Remove from pinned list if present
    bool was_pinned = false;
    for(uint8_t i = 0; i < app->pinned_count; i++) {
        if(app->pinned_kinds[i] == 0 && strcmp(app->pinned_collections[i], name) == 0) {
            was_pinned = true;
            for(uint8_t j = i; j + 1 < app->pinned_count; j++) {
                strlcpy(
                    app->pinned_collections[j],
                    app->pinned_collections[j + 1],
                    BT_REMOTES_COLLECTION_NAME_LEN);
                app->pinned_kinds[j] = app->pinned_kinds[j + 1];
            }
            app->pinned_count--;
            break;
        }
    }
    if(was_pinned) bt_remotes_pinned_save(app);

    return (err == FSE_OK || err == FSE_NOT_EXIST);
}

void bt_remotes_pinned_load(Hid* app) {
    app->pinned_count = 0;
    if(app->active_profile[0] == '\0') return;

    FuriString* path = furi_string_alloc_printf(
        "%s/%s.pins", BT_REMOTES_PROFILES_DIR, app->active_profile);

    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    FuriString*    tmp = furi_string_alloc();
    uint32_t       ver = 0;

    do {
        if(!flipper_format_file_open_existing(fff, furi_string_get_cstr(path))) break;
        if(!flipper_format_read_header(fff, tmp, &ver)) break;
        if(strcmp(furi_string_get_cstr(tmp), BT_REMOTES_PINS_FILE_TYPE) != 0) break;

        uint32_t count = 0;
        if(!flipper_format_read_uint32(fff, "count", &count, 1)) break;
        if(count > BT_REMOTES_PINNED_MAX) count = BT_REMOTES_PINNED_MAX;

        for(uint32_t i = 0; i < count; i++) {
            char key[24];
            flipper_format_rewind(fff);
            snprintf(key, sizeof(key), "collection_%u", (unsigned)i);
            if(flipper_format_read_string(fff, key, tmp)) {
                strlcpy(
                    app->pinned_collections[app->pinned_count],
                    furi_string_get_cstr(tmp),
                    BT_REMOTES_COLLECTION_NAME_LEN);
                // Per-pin kind (0=collection, 1=gesture); older files omit it and
                // default to collection for backward compatibility.
                uint32_t kind = 0;
                flipper_format_rewind(fff);
                snprintf(key, sizeof(key), "kind_%u", (unsigned)i);
                flipper_format_read_uint32(fff, key, &kind, 1);
                app->pinned_kinds[app->pinned_count] = (uint8_t)(kind ? 1 : 0);
                app->pinned_count++;
            }
        }
    } while(0);

    furi_string_free(tmp);
    flipper_format_free(fff);
    furi_string_free(path);
}

void bt_remotes_pinned_save(Hid* app) {
    if(app->active_profile[0] == '\0') return;

    FuriString* path = furi_string_alloc_printf(
        "%s/%s.pins", BT_REMOTES_PROFILES_DIR, app->active_profile);

    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    if(flipper_format_file_open_always(fff, furi_string_get_cstr(path))) {
        flipper_format_write_header_cstr(
            fff, BT_REMOTES_PINS_FILE_TYPE, BT_REMOTES_PINS_FILE_VERSION);
        uint32_t count = app->pinned_count;
        flipper_format_write_uint32(fff, "count", &count, 1);
        for(uint8_t i = 0; i < app->pinned_count; i++) {
            char key[20];
            snprintf(key, sizeof(key), "collection_%u", (unsigned)i);
            flipper_format_write_string_cstr(fff, key, app->pinned_collections[i]);
            uint32_t kind = app->pinned_kinds[i];
            snprintf(key, sizeof(key), "kind_%u", (unsigned)i);
            flipper_format_write_uint32(fff, key, &kind, 1);
        }
        flipper_format_file_close(fff);
    }
    flipper_format_free(fff);
    furi_string_free(path);
}

// ---------------------------------------------------------------------------
// Custom Gestures — global library (mirrors the collection load/save pattern)
// ---------------------------------------------------------------------------

void bt_remotes_gesture_path(const char* name, char* out, size_t out_size) {
    snprintf(out, out_size, "%s/%s%s", BT_REMOTES_GESTURE_DIR, name, BT_REMOTES_GESTURE_EXT);
}

void bt_remotes_gesture_load_list(Hid* app) {
    app->gesture_count = 0;

    File*    dir = storage_file_alloc(app->storage);
    FileInfo info;
    char     name[BT_REMOTES_GESTURE_NAME_LEN + 16];

    if(storage_dir_open(dir, BT_REMOTES_GESTURE_DIR)) {
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            if(name[0] == '.') continue;
            if(app->gesture_count >= BT_REMOTES_GESTURE_MAX) break;
            size_t len     = strlen(name);
            size_t ext_len = strlen(BT_REMOTES_GESTURE_EXT);
            if(len <= ext_len) continue;
            if(strcmp(name + len - ext_len, BT_REMOTES_GESTURE_EXT) != 0) continue;
            name[len - ext_len] = '\0';
            strlcpy(app->gesture_names[app->gesture_count], name, BT_REMOTES_GESTURE_NAME_LEN);
            app->gesture_count++;
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
}

bool bt_remotes_gesture_load(Hid* app, const char* name) {
    strlcpy(app->editing_gesture_name, name, BT_REMOTES_GESTURE_NAME_LEN);
    app->editing_gesture_line_count = 0;

    char path[256];
    bt_remotes_gesture_path(name, path, sizeof(path));

    File* file = storage_file_alloc(app->storage);
    bool  ok   = false;

    if(storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        ok = true;
        char buf[GESTURE_LINE_LEN];
        bool eof = false;
        while(app->editing_gesture_line_count < GESTURE_LINE_MAX && !eof) {
            // Read one line (strip \r, NUL-terminate, truncate over-long lines).
            size_t len = 0;
            char   c;
            while(true) {
                if(storage_file_read(file, &c, 1) != 1) {
                    eof = true;
                    break;
                }
                if(c == '\n') break;
                if(c == '\r') continue;
                if(len < sizeof(buf) - 1) buf[len++] = c;
            }
            buf[len] = '\0';
            // Skip blanks, comments, and header lines; everything else is a command.
            if(buf[0] == '\0') continue;
            if(buf[0] == '#') continue;
            if(strncmp(buf, "Filetype:", 9) == 0) continue;
            if(strncmp(buf, "Version:", 8) == 0) continue;
            strlcpy(
                app->editing_gesture_lines[app->editing_gesture_line_count],
                buf,
                GESTURE_LINE_LEN);
            app->editing_gesture_line_count++;
        }
        storage_file_close(file);
    }
    storage_file_free(file);
    return ok;
}

bool bt_remotes_gesture_save(Hid* app) {
    storage_simply_mkdir(app->storage, BT_REMOTES_GESTURE_DIR);

    char path[256];
    bt_remotes_gesture_path(app->editing_gesture_name, path, sizeof(path));

    File* file = storage_file_alloc(app->storage);
    bool  ok   = false;

    if(storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        // Plain text: one command per line, no header (the reader still tolerates
        // a legacy "Filetype:"/"Version:" header on files that have one).
        FuriString* out = furi_string_alloc();
        for(uint8_t i = 0; i < app->editing_gesture_line_count; i++) {
            furi_string_cat_printf(out, "%s\n", app->editing_gesture_lines[i]);
        }
        size_t len = furi_string_size(out);
        ok = storage_file_write(file, furi_string_get_cstr(out), len) == len;
        furi_string_free(out);
        storage_file_close(file);
    }
    storage_file_free(file);
    return ok;
}

bool bt_remotes_gesture_delete(Hid* app, const char* name) {
    char path[256];
    bt_remotes_gesture_path(name, path, sizeof(path));
    FS_Error err = storage_common_remove(app->storage, path);

    // Remove from pinned list if present (gesture-kind entries only)
    bool was_pinned = false;
    for(uint8_t i = 0; i < app->pinned_count; i++) {
        if(app->pinned_kinds[i] == 1 && strcmp(app->pinned_collections[i], name) == 0) {
            was_pinned = true;
            for(uint8_t j = i; j + 1 < app->pinned_count; j++) {
                strlcpy(
                    app->pinned_collections[j],
                    app->pinned_collections[j + 1],
                    BT_REMOTES_COLLECTION_NAME_LEN);
                app->pinned_kinds[j] = app->pinned_kinds[j + 1];
            }
            app->pinned_count--;
            break;
        }
    }
    if(was_pinned) bt_remotes_pinned_save(app);

    return (err == FSE_OK || err == FSE_NOT_EXIST);
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// Shared utilities
// ---------------------------------------------------------------------------

const char* bt_remotes_path_basename(const char* path) {
    const char* last_slash = strrchr(path, '/');
    return last_slash ? last_slash + 1 : path;
}

void bt_remotes_show_running_popup(Hid* app) {
    popup_reset(app->popup);
    popup_set_header(
        app->popup, bt_remotes_path_basename(app->pending_script_path),
        64, 3, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Running...\nPress Back to stop.", 64, 28, AlignCenter, AlignTop);
    popup_set_context(app->popup, app);
    popup_set_callback(app->popup, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, HidViewPopup);
}

bool bt_remotes_validate_name(const char* text, FuriString* error) {
    if(text[0] == '\0') {
        furi_string_set(error, "Name cannot\nbe empty");
        return false;
    }
    const char* invalid = "<>:\"/\\|?*";
    for(size_t i = 0; text[i]; i++) {
        if(strchr(invalid, text[i])) {
            furi_string_printf(error, "Char '%c' not\nallowed", text[i]);
            return false;
        }
    }
    return true;
}

// BLE lifecycle
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Post-pairing auto-save timer
// ---------------------------------------------------------------------------

// Runs on the FuriTimer thread every PAIR_SAVE_POLL_MS after a first-time BLE connect.
// Polls for .bt_hid.keys (written by the BT stack when SMP bonding completes) and saves
// the profile as soon as it appears.  Stops itself on success or after max attempts.
static void bt_remotes_pair_save_timer_cb(void* context) {
    Hid* app = context;
    app->pair_save_attempts++;
    if(bt_remotes_profile_save(app)) {
        furi_timer_stop(app->pair_save_timer);
        FURI_LOG_I(TAG, "Profile auto-saved after pairing (attempt %u)", app->pair_save_attempts);
    } else if(app->pair_save_attempts >= PAIR_SAVE_MAX_ATTEMPTS) {
        furi_timer_stop(app->pair_save_timer);
        FURI_LOG_W(TAG, "Pair-save polling timed out");
    }
}

// Connect-wait timer (Ducky "connect per run"): posts a tick so the run scene re-checks
// app->connected on the UI thread. Runs on the FuriTimer thread, so it must not touch UI.
static void bt_remotes_connect_wait_timer_cb(void* context) {
    Hid* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, BT_REMOTES_EVENT_CONNECT_TICK);
}

// Forward declaration so bt_remotes_start_ble can register it
static void bt_remotes_connection_status_changed_callback(BtStatus status, void* context);

void bt_remotes_start_ble(Hid* app) {
    furi_assert(!app->ble_started);

    // Ensure LED is off until an actual connection is established
    notification_internal_message(app->notifications, &sequence_reset_blue);

    bt_keys_storage_set_storage_path(app->bt, APP_DATA_PATH(HID_BT_KEYS_STORAGE_NAME));
    bt_set_status_changed_callback(app->bt, bt_remotes_connection_status_changed_callback, app);

    bt_remotes_load_cfg(app);

    app->ble_hid_profile = bt_profile_start(app->bt, ble_profile_hid_ext, &app->ble_hid_cfg);
    furi_check(app->ble_hid_profile);

    furi_hal_bt_start_advertising();
    app->ble_started = true;
    FURI_LOG_I(TAG, "BLE started");
}

void bt_remotes_start_ble_if_immediate(Hid* app) {
    // Immediate-connect profiles bring BLE up now; delay-connect profiles defer it
    // to the Start scene (start on remote entry, stop on return to the menu).
    if(!app->delay_connect) bt_remotes_start_ble(app);
}

void bt_remotes_ducky_browse_enter(Hid* app) {
    // Stay disconnected while browsing Ducky scripts/collections in per-run mode.
    // Call from every Ducky/Collections browsing scene on_enter so new scenes
    // enforce the policy without knowing its implementation.
    if(app->ducky_connect_per_run) bt_remotes_stop_ble(app);
}

void bt_remotes_stop_ble(Hid* app) {
    if(!app->ble_started) return;

    furi_timer_stop(app->pair_save_timer);
    bt_set_status_changed_callback(app->bt, NULL, NULL);
    notification_internal_message(app->notifications, &sequence_reset_blue);
    bt_disconnect(app->bt);
    furi_delay_ms(200);
    bt_keys_storage_set_default_path(app->bt);
    furi_check(bt_profile_restore_default(app->bt));
    app->ble_started     = false;
    app->ble_hid_profile = NULL; // profile freed by restore_default; NULL so HAL
                                 // wrappers silently no-op any stale input events
    // Callback was detached above, so the disconnect won't report back — clear the
    // link flag here so a later connect-wait doesn't see a stale "connected".
    app->connected = false;
    FURI_LOG_I(TAG, "BLE stopped");
}

// ---------------------------------------------------------------------------
// Connection status callback
// ---------------------------------------------------------------------------

static void bt_remotes_connection_status_changed_callback(BtStatus status, void* context) {
    furi_assert(context);
    Hid* hid = context;
    const bool connected = (status == BtStatusConnected);
    hid->connected = connected; // app-level link state (e.g. Ducky per-run connect wait)
    notification_internal_message(
        hid->notifications, connected ? &sequence_set_blue_255 : &sequence_reset_blue);
    if(connected && (hid->vibro_mode == 2 || hid->vibro_mode == 3)) {
        notification_message(hid->notifications, &sequence_single_vibro);
    } else if(!connected && (hid->vibro_mode == 1 || hid->vibro_mode == 3)) {
        notification_message(hid->notifications, &sequence_single_vibro);
    }

    if(hid->active_profile[0] != '\0') {
        if(connected) {
            // Check whether this profile already has saved keys on disk.
            // If yes → reconnect: keys are already correct, save immediately to capture
            //   any key material refreshed by the stack during re-bonding.
            // If no  → first-time pairing: the BLE link layer connected before SMP bonding
            //   finished, so .bt_hid.keys doesn't exist yet.  Start the polling timer;
            //   it will save as soon as the BT stack writes the keys file.
            FuriString* prof_keys = furi_string_alloc_printf(
                "%s/%s%s", BT_REMOTES_PROFILES_DIR, hid->active_profile, BT_REMOTES_KEYS_EXT);
            bool already_saved =
                storage_file_exists(hid->storage, furi_string_get_cstr(prof_keys));
            furi_string_free(prof_keys);

            if(already_saved) {
                bt_remotes_profile_save(hid);
            } else {
                hid->pair_save_attempts = 0;
                furi_timer_start(hid->pair_save_timer, PAIR_SAVE_POLL_MS);
                FURI_LOG_I(TAG, "First-time pairing — polling for keys");
            }
        } else {
            // Disconnected: cancel any in-progress pairing poll
            furi_timer_stop(hid->pair_save_timer);
        }
    }
    // Let the connect-per-run run scene check its settle condition immediately on
    // link-up. Without this, the 0 ms settle setting still incurs one full poll
    // period (~150 ms) before start_run fires. The event is a no-op in all other scenes.
    if(connected) {
        view_dispatcher_send_custom_event(hid->view_dispatcher, BT_REMOTES_EVENT_CONNECT_TICK);
    }

    hid_keynote_set_connected_status(hid->hid_keynote, connected);
    hid_keyboard_set_connected_status(hid->hid_keyboard, connected);
    hid_numpad_set_connected_status(hid->hid_numpad, connected);
    hid_media_set_connected_status(hid->hid_media, connected);
    hid_music_macos_set_connected_status(hid->hid_music_macos, connected);
    hid_movie_set_connected_status(hid->hid_movie, connected);
    hid_mouse_set_connected_status(hid->hid_mouse, connected);
    hid_mouse_clicker_set_connected_status(hid->hid_mouse_clicker, connected);
    hid_mouse_jiggler_set_connected_status(hid->hid_mouse_jiggler, connected);
    hid_mouse_jiggler_stealth_set_connected_status(hid->hid_mouse_jiggler_stealth, connected);
    hid_ptt_set_connected_status(hid->hid_ptt, connected);
    hid_tiktok_set_connected_status(hid->hid_tiktok, connected);
}

// ---------------------------------------------------------------------------
// App alloc / free / entry
// ---------------------------------------------------------------------------

static bool bt_remotes_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    Hid* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool bt_remotes_back_event_callback(void* context) {
    furi_assert(context);
    Hid* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static uint32_t hid_ptt_menu_view(void* context) {
    UNUSED(context);
    return HidViewPushToTalkMenu;
}

static Hid* bt_remotes_alloc(void) {
    Hid* app = malloc(sizeof(Hid));
    memset(app, 0, sizeof(Hid));

    app->gui = furi_record_open(RECORD_GUI);
    app->bt = furi_record_open(RECORD_BT);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, bt_remotes_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bt_remotes_back_event_callback);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->scene_manager = scene_manager_alloc(&bt_remotes_scene_handlers, app);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewSubmenu, submenu_get_view(app->submenu));
    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidViewVariableItemList,
        variable_item_list_get_view(app->var_item_list));

    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewDialog, dialog_ex_get_view(app->dialog));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewTextInput, text_input_get_view(app->text_input));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewPopup, popup_get_view(app->popup));

    app->help_widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewHelp, widget_get_view(app->help_widget));

    app->hid_keynote = hid_keynote_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewKeynote, hid_keynote_get_view(app->hid_keynote));

    app->hid_keyboard = hid_keyboard_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewKeyboard, hid_keyboard_get_view(app->hid_keyboard));

    app->hid_numpad = hid_numpad_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewNumpad, hid_numpad_get_view(app->hid_numpad));

    app->hid_media = hid_media_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewMedia, hid_media_get_view(app->hid_media));

    app->hid_music_macos = hid_music_macos_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidViewMusicMacOs,
        hid_music_macos_get_view(app->hid_music_macos));

    app->hid_movie = hid_movie_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewMovie, hid_movie_get_view(app->hid_movie));

    app->hid_tiktok = hid_tiktok_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, BtHidViewTikTok, hid_tiktok_get_view(app->hid_tiktok));

    app->hid_mouse = hid_mouse_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewMouse, hid_mouse_get_view(app->hid_mouse));

    app->hid_mouse_clicker = hid_mouse_clicker_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidViewMouseClicker,
        hid_mouse_clicker_get_view(app->hid_mouse_clicker));

    app->hid_mouse_jiggler = hid_mouse_jiggler_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidViewMouseJiggler,
        hid_mouse_jiggler_get_view(app->hid_mouse_jiggler));

    app->hid_mouse_jiggler_stealth = hid_mouse_jiggler_stealth_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidViewMouseJigglerStealth,
        hid_mouse_jiggler_stealth_get_view(app->hid_mouse_jiggler_stealth));

    app->hid_ptt_menu = hid_ptt_menu_alloc(app);
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidViewPushToTalkMenu,
        hid_ptt_menu_get_view(app->hid_ptt_menu));
    app->hid_ptt = hid_ptt_alloc(app);
    view_set_previous_callback(hid_ptt_get_view(app->hid_ptt), hid_ptt_menu_view);
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewPushToTalk, hid_ptt_get_view(app->hid_ptt));

    app->hid_remote_menu = hid_remote_menu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidViewRemoteMenu,
        hid_remote_menu_get_view(app->hid_remote_menu));

    app->file_browser_result = furi_string_alloc();
    app->file_browser        = file_browser_alloc(app->file_browser_result);
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidViewFileBrowser,
        file_browser_get_view(app->file_browser));

    app->ducky_runner = ducky_runner_alloc();
    app->gesture_runner = gesture_runner_alloc();

    app->pair_save_timer =
        furi_timer_alloc(bt_remotes_pair_save_timer_cb, FuriTimerTypePeriodic, app);
    app->connect_wait_timer =
        furi_timer_alloc(bt_remotes_connect_wait_timer_cb, FuriTimerTypePeriodic, app);

    return app;
}

static void bt_remotes_free(Hid* app) {
    furi_assert(app);

    if(app->ble_started) {
        bt_remotes_stop_ble(app);
    }

    notification_internal_message(app->notifications, &sequence_reset_blue);

    view_dispatcher_remove_view(app->view_dispatcher, HidViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewVariableItemList);
    variable_item_list_free(app->var_item_list);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewDialog);
    dialog_ex_free(app->dialog);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewTextInput);
    text_input_free(app->text_input);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewPopup);
    popup_free(app->popup);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewHelp);
    widget_free(app->help_widget);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewKeynote);
    hid_keynote_free(app->hid_keynote);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewKeyboard);
    hid_keyboard_free(app->hid_keyboard);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewNumpad);
    hid_numpad_free(app->hid_numpad);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewMedia);
    hid_media_free(app->hid_media);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewMusicMacOs);
    hid_music_macos_free(app->hid_music_macos);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewMovie);
    hid_movie_free(app->hid_movie);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewMouse);
    hid_mouse_free(app->hid_mouse);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewMouseClicker);
    hid_mouse_clicker_free(app->hid_mouse_clicker);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewMouseJiggler);
    hid_mouse_jiggler_free(app->hid_mouse_jiggler);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewMouseJigglerStealth);
    hid_mouse_jiggler_stealth_free(app->hid_mouse_jiggler_stealth);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewPushToTalkMenu);
    hid_ptt_menu_free(app->hid_ptt_menu);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewPushToTalk);
    hid_ptt_free(app->hid_ptt);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewRemoteMenu);
    hid_remote_menu_free(app->hid_remote_menu);
    view_dispatcher_remove_view(app->view_dispatcher, BtHidViewTikTok);
    hid_tiktok_free(app->hid_tiktok);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewFileBrowser);
    file_browser_free(app->file_browser);
    furi_string_free(app->file_browser_result);
    ducky_runner_free(app->ducky_runner);
    gesture_runner_free(app->gesture_runner);
    furi_timer_free(app->pair_save_timer);
    furi_timer_free(app->connect_wait_timer);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_BT);
    furi_record_close(RECORD_GUI);

    free(app);
}

// This app targets Momentum firmware. It doesn't enforce that at runtime: it
// already fails to load on stock/Unleashed/RogueMaster because it imports several
// app-API symbols only Momentum exports (a set of built-in icons plus strtok /
// variable_item_list_set_header), so those firmwares reject it with "Update
// Firmware to use with this Application". See docs/ARCHITECTURE.md → Firmware
// Compatibility for the full analysis and the path to broader compatibility.
int32_t bt_remotes_app(void* p) {
    UNUSED(p);

    Hid* app = bt_remotes_alloc();

    notification_internal_message(app->notifications, &sequence_reset_blue);

    bt_disconnect(app->bt);
    furi_delay_ms(200);

    // Migrate legacy sd-card keys if present
    storage_common_migrate(
        app->storage,
        EXT_PATH("apps/Tools/.bt_hid.keys"),
        APP_DATA_PATH(HID_BT_KEYS_STORAGE_NAME));

    storage_simply_mkdir(app->storage, BT_REMOTES_PROFILES_DIR);
    storage_simply_mkdir(app->storage, BT_REMOTES_COLLECTION_DIR);

    // Load app-level config (default BT name for new profiles)
    bt_remotes_load_app_cfg(app);

    dolphin_deed(DolphinDeedPluginStart);

    scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileSelect);

    view_dispatcher_run(app->view_dispatcher);

    bt_set_status_changed_callback(app->bt, NULL, NULL);

    bt_remotes_free(app);

    return 0;
}
