#include "bt_remotes.h"
#include "views.h"
#include <flipper_format/flipper_format.h>
#include <furi_hal_random.h>
#include <notification/notification_messages.h>
#include <dolphin/dolphin.h>

#define TAG "BtRemotes"

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
        uint32_t order_u32[BT_REMOTES_MENU_ITEM_COUNT];
        for(uint8_t i = 0; i < BT_REMOTES_MENU_ITEM_COUNT; i++) order_u32[i] = app->menu_order[i];
        flipper_format_write_uint32(fff, "menu_order", order_u32, BT_REMOTES_MENU_ITEM_COUNT);
        uint32_t hidden_u32 = app->menu_hidden;
        flipper_format_write_uint32(fff, "menu_hidden", &hidden_u32, 1);
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
        for(uint8_t i = 0; i < BT_REMOTES_MENU_ITEM_COUNT; i++) app->menu_order[i] = i;
        app->menu_hidden = 0;

        FlipperFormat* mfff = flipper_format_file_alloc(app->storage);
        FuriString*    mtmp = furi_string_alloc();
        uint32_t       mver = 0;
        do {
            if(!flipper_format_file_open_existing(mfff, furi_string_get_cstr(src_cfg))) break;
            if(!flipper_format_read_header(mfff, mtmp, &mver)) break;

            uint32_t order_u32[BT_REMOTES_MENU_ITEM_COUNT];
            for(uint8_t i = 0; i < BT_REMOTES_MENU_ITEM_COUNT; i++) order_u32[i] = i;
            if(flipper_format_read_uint32(
                   mfff, "menu_order", order_u32, BT_REMOTES_MENU_ITEM_COUNT)) {
                for(uint8_t i = 0; i < BT_REMOTES_MENU_ITEM_COUNT; i++) {
                    app->menu_order[i] = (uint8_t)(
                        order_u32[i] < BT_REMOTES_MENU_ITEM_COUNT ? order_u32[i] : i);
                }
            }
            flipper_format_rewind(mfff);
            uint32_t hidden_u32 = 0;
            if(flipper_format_read_uint32(mfff, "menu_hidden", &hidden_u32, 1)) {
                app->menu_hidden = hidden_u32;
                // Settings and Custom Remotes items must never be hidden
                app->menu_hidden &= ~(uint32_t)(1u << (BT_REMOTES_MENU_ITEM_COUNT - 1));
                app->menu_hidden &= ~(uint32_t)(1u << (BT_REMOTES_MENU_ITEM_COUNT - 2));
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
            storage_common_copy(
                app->storage,
                furi_string_get_cstr(old_keys),
                furi_string_get_cstr(new_keys));
            storage_common_remove(app->storage, furi_string_get_cstr(old_keys));
        }

        FURI_LOG_I(TAG, "Profile renamed: %s → %s", app->pending_name, app->active_profile);
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
// Custom Remote operations
// ---------------------------------------------------------------------------

#define BT_REMOTES_CUSTOM_REMOTE_FILE_TYPE    "Flipper BT Custom Remote"
#define BT_REMOTES_CUSTOM_REMOTE_FILE_VERSION (1)
#define BT_REMOTES_ACTIVE_REMOTES_FILE_TYPE    "Flipper BT Custom Remote List"
#define BT_REMOTES_ACTIVE_REMOTES_FILE_VERSION (1)

// FlipperFormat key names for each input slot (indexed by CustomRemoteInputSlot)
static const char* const cr_slot_keys[CustomRemoteInputCount] = {
    "tap_up",   "tap_down",  "tap_left",  "tap_right",
    "hold_up",  "hold_down", "hold_left", "hold_right",
    "tap_ok",   "hold_ok",   "tap_back",
};

void bt_remotes_custom_remote_load_list(Hid* app) {
    app->custom_remote_count = 0;
    storage_simply_mkdir(app->storage, BT_REMOTES_CUSTOM_REMOTE_DIR);

    File*    dir = storage_file_alloc(app->storage);
    FileInfo info;
    char     name[BT_REMOTES_CUSTOM_REMOTE_NAME_LEN + 8];

    if(storage_dir_open(dir, BT_REMOTES_CUSTOM_REMOTE_DIR)) {
        while(storage_dir_read(dir, &info, name, sizeof(name))) {
            if(info.flags & FSF_DIRECTORY) continue;
            if(name[0] == '.') continue;
            if(app->custom_remote_count >= BT_REMOTES_CUSTOM_REMOTE_MAX) break;

            size_t len     = strlen(name);
            size_t ext_len = strlen(BT_REMOTES_CUSTOM_REMOTE_EXT);
            if(len <= ext_len) continue;
            if(strcmp(name + len - ext_len, BT_REMOTES_CUSTOM_REMOTE_EXT) != 0) continue;

            name[len - ext_len] = '\0';
            strlcpy(
                app->custom_remote_names[app->custom_remote_count],
                name,
                BT_REMOTES_CUSTOM_REMOTE_NAME_LEN);
            app->custom_remote_count++;
        }
        storage_dir_close(dir);
    }
    storage_file_free(dir);
    FURI_LOG_D(TAG, "Custom remote list: %u remotes", app->custom_remote_count);
}

bool bt_remotes_custom_remote_load(Hid* app, const char* name) {
    strlcpy(app->editing_remote.name, name, BT_REMOTES_CUSTOM_REMOTE_NAME_LEN);
    memset(app->editing_remote.scripts, 0, sizeof(app->editing_remote.scripts));

    FuriString* path = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_CUSTOM_REMOTE_DIR, name, BT_REMOTES_CUSTOM_REMOTE_EXT);

    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    FuriString*    tmp = furi_string_alloc();
    uint32_t       ver = 0;
    bool           ok  = false;

    do {
        if(!flipper_format_file_open_existing(fff, furi_string_get_cstr(path))) break;
        if(!flipper_format_read_header(fff, tmp, &ver)) break;
        if(strcmp(furi_string_get_cstr(tmp), BT_REMOTES_CUSTOM_REMOTE_FILE_TYPE) != 0) break;
        ok = true;
        for(uint8_t i = 0; i < CustomRemoteInputCount; i++) {
            flipper_format_rewind(fff);
            if(flipper_format_read_string(fff, cr_slot_keys[i], tmp)) {
                strlcpy(
                    app->editing_remote.scripts[i],
                    furi_string_get_cstr(tmp),
                    BT_REMOTES_CUSTOM_REMOTE_SCRIPT_LEN);
            }
        }
    } while(0);

    furi_string_free(tmp);
    flipper_format_free(fff);
    furi_string_free(path);
    if(!ok) FURI_LOG_E(TAG, "Custom remote load failed: %s", name);
    return ok;
}

bool bt_remotes_custom_remote_save(Hid* app) {
    storage_simply_mkdir(app->storage, BT_REMOTES_CUSTOM_REMOTE_DIR);

    FuriString* path = furi_string_alloc_printf(
        "%s/%s%s",
        BT_REMOTES_CUSTOM_REMOTE_DIR,
        app->editing_remote.name,
        BT_REMOTES_CUSTOM_REMOTE_EXT);

    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    bool           ok  = false;

    if(flipper_format_file_open_always(fff, furi_string_get_cstr(path))) {
        ok = flipper_format_write_header_cstr(
            fff, BT_REMOTES_CUSTOM_REMOTE_FILE_TYPE, BT_REMOTES_CUSTOM_REMOTE_FILE_VERSION);
        for(uint8_t i = 0; ok && i < CustomRemoteInputCount; i++) {
            ok = flipper_format_write_string_cstr(
                fff, cr_slot_keys[i], app->editing_remote.scripts[i]);
        }
        flipper_format_file_close(fff);
    }
    flipper_format_free(fff);
    furi_string_free(path);
    if(!ok) FURI_LOG_E(TAG, "Custom remote save failed: %s", app->editing_remote.name);
    return ok;
}

void bt_remotes_active_remotes_save(Hid* app) {
    if(app->active_profile[0] == '\0') return;

    FuriString* path = furi_string_alloc_printf(
        "%s/%s.remotes", BT_REMOTES_PROFILES_DIR, app->active_profile);

    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    if(flipper_format_file_open_always(fff, furi_string_get_cstr(path))) {
        flipper_format_write_header_cstr(
            fff, BT_REMOTES_ACTIVE_REMOTES_FILE_TYPE, BT_REMOTES_ACTIVE_REMOTES_FILE_VERSION);
        uint32_t count = app->active_custom_remote_count;
        flipper_format_write_uint32(fff, "count", &count, 1);
        for(uint8_t i = 0; i < app->active_custom_remote_count; i++) {
            char key[20];
            snprintf(key, sizeof(key), "remote_%u", (unsigned)i);
            flipper_format_write_string_cstr(fff, key, app->active_custom_remotes[i]);
        }
        flipper_format_file_close(fff);
    }
    flipper_format_free(fff);
    furi_string_free(path);
}

bool bt_remotes_custom_remote_delete(Hid* app, const char* name) {
    FuriString* path = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_CUSTOM_REMOTE_DIR, name, BT_REMOTES_CUSTOM_REMOTE_EXT);
    FS_Error err = storage_common_remove(app->storage, furi_string_get_cstr(path));
    furi_string_free(path);

    // Remove from active list if present
    bool was_active = false;
    for(uint8_t i = 0; i < app->active_custom_remote_count; i++) {
        if(strcmp(app->active_custom_remotes[i], name) == 0) {
            was_active = true;
            for(uint8_t j = i; j + 1 < app->active_custom_remote_count; j++) {
                strlcpy(
                    app->active_custom_remotes[j],
                    app->active_custom_remotes[j + 1],
                    BT_REMOTES_CUSTOM_REMOTE_NAME_LEN);
            }
            app->active_custom_remote_count--;
            break;
        }
    }
    if(was_active) bt_remotes_active_remotes_save(app);

    return (err == FSE_OK || err == FSE_NOT_EXIST);
}

void bt_remotes_active_remotes_load(Hid* app) {
    app->active_custom_remote_count = 0;
    if(app->active_profile[0] == '\0') return;

    FuriString* path = furi_string_alloc_printf(
        "%s/%s.remotes", BT_REMOTES_PROFILES_DIR, app->active_profile);

    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    FuriString*    tmp = furi_string_alloc();
    uint32_t       ver = 0;

    do {
        if(!flipper_format_file_open_existing(fff, furi_string_get_cstr(path))) break;
        if(!flipper_format_read_header(fff, tmp, &ver)) break;
        if(strcmp(furi_string_get_cstr(tmp), BT_REMOTES_ACTIVE_REMOTES_FILE_TYPE) != 0) break;

        uint32_t count = 0;
        if(!flipper_format_read_uint32(fff, "count", &count, 1)) break;
        if(count > BT_REMOTES_CUSTOM_REMOTE_MAX) count = BT_REMOTES_CUSTOM_REMOTE_MAX;

        for(uint32_t i = 0; i < count; i++) {
            char key[20];
            snprintf(key, sizeof(key), "remote_%u", (unsigned)i);
            flipper_format_rewind(fff);
            if(flipper_format_read_string(fff, key, tmp)) {
                strlcpy(
                    app->active_custom_remotes[app->active_custom_remote_count],
                    furi_string_get_cstr(tmp),
                    BT_REMOTES_CUSTOM_REMOTE_NAME_LEN);
                app->active_custom_remote_count++;
            }
        }
    } while(0);

    furi_string_free(tmp);
    flipper_format_free(fff);
    furi_string_free(path);
    FURI_LOG_D(TAG, "Active remotes loaded: %u", app->active_custom_remote_count);
}

// ---------------------------------------------------------------------------
// BLE lifecycle
// ---------------------------------------------------------------------------

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

void bt_remotes_stop_ble(Hid* app) {
    if(!app->ble_started) return;

    bt_set_status_changed_callback(app->bt, NULL, NULL);
    notification_internal_message(app->notifications, &sequence_reset_blue);
    bt_disconnect(app->bt);
    furi_delay_ms(200);
    bt_keys_storage_set_default_path(app->bt);
    furi_check(bt_profile_restore_default(app->bt));
    app->ble_started = false;
    FURI_LOG_I(TAG, "BLE stopped");
}

// ---------------------------------------------------------------------------
// Connection status callback
// ---------------------------------------------------------------------------

static void bt_remotes_connection_status_changed_callback(BtStatus status, void* context) {
    furi_assert(context);
    Hid* hid = context;
    const bool connected = (status == BtStatusConnected);
    notification_internal_message(
        hid->notifications, connected ? &sequence_set_blue_255 : &sequence_reset_blue);
    if(connected && (hid->vibro_mode == 2 || hid->vibro_mode == 3)) {
        // Connect or Both
        notification_message(hid->notifications, &sequence_single_vibro);
    } else if(!connected && (hid->vibro_mode == 1 || hid->vibro_mode == 3)) {
        // Disconnect or Both
        notification_message(hid->notifications, &sequence_single_vibro);
    }
    // Save on every status transition while a profile is active.
    //
    // On a *reconnect* the keys file already exists when BtStatusConnected fires,
    // so the save succeeds there.
    //
    // On a *first-time pairing* the BLE link layer connects before SMP
    // bonding completes, so BtStatusConnected fires before the keys file is
    // written.  The second transition (connected → advertising, i.e. disconnect)
    // fires after bonding is complete and the keys are on disk, so saving there
    // catches the first-time case.
    //
    // bt_remotes_profile_save is a safe no-op (returns false immediately) when
    // the keys file does not exist, so calling it unconditionally is harmless.
    if(hid->active_profile[0] != '\0') {
        bt_remotes_profile_save(hid);
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

    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewDialog, dialog_ex_get_view(app->dialog));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewTextInput, text_input_get_view(app->text_input));

    app->popup = popup_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, HidViewPopup, popup_get_view(app->popup));

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

    app->hid_custom_remote = hid_custom_remote_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        HidViewCustomRemote,
        hid_custom_remote_get_view(app->hid_custom_remote));

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
    view_dispatcher_remove_view(app->view_dispatcher, HidViewDialog);
    dialog_ex_free(app->dialog);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewTextInput);
    text_input_free(app->text_input);
    view_dispatcher_remove_view(app->view_dispatcher, HidViewPopup);
    popup_free(app->popup);
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
    view_dispatcher_remove_view(app->view_dispatcher, HidViewCustomRemote);
    hid_custom_remote_free(app->hid_custom_remote);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_BT);
    furi_record_close(RECORD_GUI);

    free(app);
}

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
    storage_simply_mkdir(app->storage, BT_REMOTES_CUSTOM_REMOTE_DIR);

    // Load app-level config (default BT name for new profiles)
    bt_remotes_load_app_cfg(app);

    dolphin_deed(DolphinDeedPluginStart);

    scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileSelect);

    view_dispatcher_run(app->view_dispatcher);

    bt_set_status_changed_callback(app->bt, NULL, NULL);

    bt_remotes_free(app);

    return 0;
}
