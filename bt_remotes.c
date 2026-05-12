#include "bt_remotes.h"
#include "views.h"
#include <flipper_format/flipper_format.h>
#include <furi_hal_random.h>
#include <notification/notification_messages.h>
#include <dolphin/dolphin.h>

#define TAG "BtRemotes"

#define BT_REMOTES_CFG_FILE_TYPE "Flipper BT Remote Settings File"
#define BT_REMOTES_CFG_VERSION   (1)
#define BT_REMOTES_MAC_SIZE      (6)

// ---------------------------------------------------------------------------
// Config helpers
// ---------------------------------------------------------------------------

static void bt_remotes_write_cfg(Hid* app, const char* path, uint8_t mac[BT_REMOTES_MAC_SIZE]) {
    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    if(flipper_format_file_open_always(fff, path)) {
        flipper_format_write_header_cstr(fff, BT_REMOTES_CFG_FILE_TYPE, BT_REMOTES_CFG_VERSION);
        flipper_format_write_string_cstr(fff, "name", "");
        flipper_format_write_hex(fff, "mac", mac, BT_REMOTES_MAC_SIZE);
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
}

bool bt_remotes_profile_create(Hid* app) {
    storage_simply_mkdir(app->storage, BT_REMOTES_PROFILES_DIR);

    // Generate static-random BLE public address (bits 7:6 of byte[5] = 11)
    uint8_t mac[BT_REMOTES_MAC_SIZE];
    furi_hal_random_fill_buf(mac, BT_REMOTES_MAC_SIZE);
    mac[5] |= 0xC0;

    // Write profile cfg
    FuriString* dst = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->active_profile, BT_REMOTES_CFG_EXT);
    bool ok = false;
    FlipperFormat* fff = flipper_format_file_alloc(app->storage);
    if(flipper_format_file_open_always(fff, furi_string_get_cstr(dst))) {
        ok = flipper_format_write_header_cstr(
                 fff, BT_REMOTES_CFG_FILE_TYPE, BT_REMOTES_CFG_VERSION) &&
             flipper_format_write_string_cstr(fff, "name", "") &&
             flipper_format_write_hex(fff, "mac", mac, BT_REMOTES_MAC_SIZE);
        flipper_format_file_close(fff);
    }
    flipper_format_free(fff);
    furi_string_free(dst);

    if(ok) {
        // Push the new MAC into the active cfg so BLE starts with it
        bt_remotes_write_cfg(app, BT_REMOTES_CFG_PATH, mac);
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

    if(err == FSE_OK) {
        FURI_LOG_I(TAG, "Profile saved: %s", app->active_profile);
    } else {
        FURI_LOG_E(TAG, "Profile save failed: %d", err);
    }
    return err == FSE_OK;
}

bool bt_remotes_profile_activate(Hid* app) {
    FuriString* src_cfg = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->active_profile, BT_REMOTES_CFG_EXT);
    FuriString* src_keys = furi_string_alloc_printf(
        "%s/%s%s", BT_REMOTES_PROFILES_DIR, app->active_profile, BT_REMOTES_KEYS_EXT);

    // Restore cfg (contains the profile's MAC)
    if(storage_file_exists(app->storage, furi_string_get_cstr(src_cfg))) {
        storage_common_remove(app->storage, BT_REMOTES_CFG_PATH);
        storage_common_copy(
            app->storage, furi_string_get_cstr(src_cfg), BT_REMOTES_CFG_PATH);
    }

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

// ---------------------------------------------------------------------------
// BLE lifecycle
// ---------------------------------------------------------------------------

void bt_remotes_start_ble(Hid* app) {
    furi_assert(!app->ble_started);

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
    view_dispatcher_remove_view(app->view_dispatcher, BtHidViewTikTok);
    hid_tiktok_free(app->hid_tiktok);

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

    bt_disconnect(app->bt);
    furi_delay_ms(200);

    // Migrate legacy sd-card keys if present
    storage_common_migrate(
        app->storage,
        EXT_PATH("apps/Tools/.bt_hid.keys"),
        APP_DATA_PATH(HID_BT_KEYS_STORAGE_NAME));

    bt_keys_storage_set_storage_path(app->bt, APP_DATA_PATH(HID_BT_KEYS_STORAGE_NAME));

    storage_simply_mkdir(app->storage, BT_REMOTES_PROFILES_DIR);

    bt_set_status_changed_callback(app->bt, bt_remotes_connection_status_changed_callback, app);

    dolphin_deed(DolphinDeedPluginStart);

    scene_manager_next_scene(app->scene_manager, BtRemotesSceneProfileSelect);

    view_dispatcher_run(app->view_dispatcher);

    bt_set_status_changed_callback(app->bt, NULL, NULL);

    bt_remotes_free(app);

    return 0;
}
