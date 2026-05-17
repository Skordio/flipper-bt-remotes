# bt_remotes — Agent Knowledge Base

This file is the canonical reference for AI agents working on the `bt_remotes` Flipper Zero
application. Read it fully before making any changes.

---

## Project Overview

`bt_remotes` is a Bluetooth HID remote-control app for the Flipper Zero running Momentum
Firmware. It lets the Flipper act as a BLE HID peripheral (keyboard, mouse, media controller,
etc.) and supports multiple named profiles, each with its own BLE MAC address and bonding keys,
so the Flipper can quickly switch between paired hosts.

The repo lives at `applications_user/bt_remotes/` inside the Momentum Firmware tree and has its
own independent git remote at `https://github.com/Skordio/flipper-bt-remotes.git`.

**To commit and push: always run git commands with `-C` targeting the `bt_remotes/` directory,
not the Momentum Firmware root.**

```sh
git -C /path/to/bt_remotes status
git -C /path/to/bt_remotes push
```

---

## Repository Layout

```
bt_remotes/
├── bt_remotes.c              # Core logic: BLE lifecycle, profile ops, config I/O
├── bt_remotes.h              # Hid struct, all constants, all public function decls
├── views.h                   # HidView enum (all view IDs used with view_dispatcher)
├── application.fam           # Build metadata
├── helpers/
│   └── ble_hid_ext_profile.h # BleProfileHidExtParams (name[20], mac[6])
├── scenes/
│   ├── bt_remotes_scenes.h   # ADD_SCENE macro list — one line per scene
│   ├── bt_remotes_scene.c/h  # Generated scene dispatch table
│   ├── bt_remotes_scene_profile_select.c
│   ├── bt_remotes_scene_profile_new.c
│   ├── bt_remotes_scene_start.c
│   ├── bt_remotes_scene_main.c
│   ├── bt_remotes_scene_settings.c
│   ├── bt_remotes_scene_rename.c
│   ├── bt_remotes_scene_profile_rename_file.c
│   ├── bt_remotes_scene_reset_profile.c
│   ├── bt_remotes_scene_save_profile.c
│   ├── bt_remotes_scene_delete_profile.c
│   └── bt_remotes_scene_unpair.c
└── views/
    ├── hid_keyboard.c/h
    ├── hid_keynote.c/h
    ├── hid_media.c/h
    ├── hid_mouse.c/h
    ├── hid_mouse_clicker.c/h
    ├── hid_mouse_jiggler.c/h
    ├── hid_mouse_jiggler_stealth.c/h
    ├── hid_movie.c/h
    ├── hid_music_macos.c/h
    ├── hid_numpad.c/h
    ├── hid_ptt.c/h
    ├── hid_ptt_menu.c/h
    └── hid_tiktok.c/h
```

---

## Key Constants (`bt_remotes.h`)

| Constant | Value | Purpose |
|---|---|---|
| `BT_REMOTES_PROFILES_DIR` | `APP_DATA_PATH("profiles")` | Profile storage directory |
| `BT_REMOTES_CFG_PATH` | `APP_DATA_PATH(".bt_hid.cfg")` | Active profile cfg (MAC + name) |
| `BT_REMOTES_APP_CFG_PATH` | `APP_DATA_PATH("app.cfg")` | App-level config (default BT name) |
| `BT_REMOTES_CFG_EXT` | `.cfg` | Profile config file extension |
| `BT_REMOTES_KEYS_EXT` | `.keys` | Profile bonding keys file extension |
| `BT_REMOTES_PROFILE_NAME_LEN` | `32` | Max profile name length (including NUL) |
| `BT_REMOTES_PROFILE_MAX_COUNT` | `16` | Max number of profiles |
| `HID_BT_KEYS_STORAGE_NAME` | `.bt_hid.keys` | Active bonding keys filename |
| `FURI_HAL_BT_ADV_NAME_LENGTH` | `20` | Max BLE device name length (from furi_hal_version.h) |

---

## The `Hid` Struct (key fields)

```c
struct Hid {
    BleProfileHidExtParams ble_hid_cfg;   // .name[20], .mac[6] — loaded from .bt_hid.cfg
    bool ble_started;                      // true while HID profile is running
    Bt* bt;                                // BT service handle
    Storage* storage;
    // ... GUI widgets ...

    // Profile management
    char active_profile[32];    // filename stem of the active profile; "" = none selected
    char pending_name[32];      // old profile name held during a rename operation
    char default_ble_name[20];  // default BT name applied when creating new profiles
    char profile_list[16][32];  // list of profile names loaded from disk
    uint8_t profile_count;
};
```

---

## Profile File Format

Each profile is stored as two files in `APP_DATA_PATH("profiles/")`:

- **`{name}.cfg`** — FlipperFormat text file:
  ```
  Filetype: Flipper BT Remote Settings File
  Version: 1
  name: My Keyboard
  mac: C0 AB CD EF 12 34
  ```
- **`{name}.keys`** — BLE bonding keys binary blob (written by the BT service)

The **active** profile's cfg is always mirrored to `APP_DATA_PATH(".bt_hid.cfg")` and keys to
`APP_DATA_PATH(".bt_hid.keys")` — these are what the BT stack actually reads at runtime.

### App-level config

`APP_DATA_PATH("app.cfg")` stores the default BT name that new profiles inherit:
```
Filetype: Flipper BT Remotes App Config
Version: 1
default_name: Flipper Zero
```

If this file doesn't exist yet, `default_ble_name` is an empty string (new profiles advertise
with no name until the user sets one).

---

## BLE Lifecycle

### `bt_remotes_start_ble(Hid* app)`

1. Sets the BT keys storage path to `APP_DATA_PATH(".bt_hid.keys")`
2. Registers `bt_remotes_connection_status_changed_callback`
3. Loads cfg from `.bt_hid.cfg` into `app->ble_hid_cfg`
4. Starts the HID profile with `bt_profile_start`
5. Starts advertising
6. Sets `app->ble_started = true`

**This function registers the status callback every time it is called.** The callback was
previously only registered once at app startup, which meant it was lost after any stop/start
cycle. This was fixed — do not revert this.

### `bt_remotes_stop_ble(Hid* app)`

1. Clears the status callback (`bt_set_status_changed_callback(NULL, NULL)`)
2. Disconnects (`bt_disconnect`)
3. Waits 200 ms
4. Resets keys path to default (`bt_keys_storage_set_default_path`)
5. Restores default BT profile (`bt_profile_restore_default`)
6. Sets `app->ble_started = false`

Has a `if(!app->ble_started) return;` guard — safe to call redundantly.

### Connection Status Callback

```c
static void bt_remotes_connection_status_changed_callback(BtStatus status, void* context)
```

Fires on every BT status change. When `status == BtStatusConnected`:
- Sets the blue LED
- Calls `bt_remotes_profile_save(app)` **if** `active_profile[0] != '\0'`
- Updates connected state in all remote-control views

**Auto-save timing is safe**: The BT service message queue processes
`BtMessageTypeKeysStorageUpdated` (which writes the keys file to disk) before
`BtMessageTypeUpdateStatus` (which fires our callback). By the time `BtStatusConnected`
arrives, the keys file is guaranteed to exist on disk.

---

## Scene Navigation Map

```
ProfileSelect ──► ProfileNew ──► (back to ProfileSelect, auto-advance to Start)
    │
    ├──► Start ◄──────────────────────────────────────────────────────────────────┐
    │      │  [Back] stops BLE, clears active_profile, returns to ProfileSelect   │
    │      │                                                                       │
    │      ├──► Main (remote-control views: Keynote, Keyboard, Mouse, etc.)       │
    │      │      [Back from Main returns to Start]                               │
    │      │                                                                       │
    │      └──► Settings ──► Rename (BT name, per-profile or default)            │
    │                   ├──► ProfileRenameFile (rename profile filename)          │
    │                   ├──► ResetProfile (new MAC + wipe pairings)               │
    │                   ├──► Unpair (clear bonding keys, keep MAC)                │
    │                   ├──► SaveProfile (snapshot keys+cfg to profile dir)       │
    │                   └──► DeleteProfile ──────────────────────────────────────►┘
    │                         (deletes profile files, clears active_profile,
    │                          navigates directly to ProfileSelect)
    │
    └──► Settings (same scene, entered without a profile; only shows BT Name)
```

**Critical navigation rule**: `bt_remotes_stop_ble` is called in `start_on_event` when
navigating to Settings. `bt_remotes_start_ble` is called in `settings_on_event` when handling
`SceneManagerEventTypeBack` — but only if `!app->ble_started && active_profile[0] != '\0'`.
The `settings_on_event` Back handler returns `false` so the scene manager still pops the scene.

---

## Adding a New Scene

1. Add a `.c` file in `scenes/` following the `on_enter` / `on_event` / `on_exit` pattern.
2. Add `ADD_SCENE(bt_remotes, my_scene, MyScene)` to `scenes/bt_remotes_scenes.h`.
3. Add the enum value `BtRemotesSceneMyScene` is auto-generated from the macro.
4. Wire navigation from an existing scene using `scene_manager_next_scene` or
   `scene_manager_search_and_switch_to_previous_scene`.
5. If the scene needs a new view, add the view ID to `views.h`, allocate the view in
   `bt_remotes_alloc`, and free it in `bt_remotes_free`.

---

## Profile Operations Reference

| Function | What it does |
|---|---|
| `bt_remotes_profile_create(app)` | Generates new random MAC, writes profile `.cfg` with default name, clears keys |
| `bt_remotes_profile_activate(app)` | Copies profile `.cfg` → `.bt_hid.cfg`, profile `.keys` → `.bt_hid.keys` (or clears keys if profile has none). Returns `false` if `.cfg` missing. |
| `bt_remotes_profile_save(app)` | Copies `.bt_hid.keys` → profile `.keys`, `.bt_hid.cfg` → profile `.cfg`. Returns `false` if no keys file exists yet. |
| `bt_remotes_profile_rename(app)` | Renames profile files. Old name in `app->pending_name`, new name in `app->active_profile`. |
| `bt_remotes_profile_reset(app)` | Generates new random MAC, saves to both cfgs, deletes both key files. |
| `bt_remotes_profile_delete(app)` | Removes profile `.cfg` and `.keys` files from the profiles directory. |
| `bt_remotes_profile_clear_pairing(app)` | Deletes both key files (active + profile) without touching BLE stack. Use this instead of `bt_hid_remove_pairing` whenever BLE is stopped. |
| `bt_remotes_load_app_cfg(app)` | Reads `app.cfg` into `app->default_ble_name`. Silent no-op if file missing. |
| `bt_remotes_save_app_cfg(app)` | Writes `app->default_ble_name` to `app.cfg`. |

**Static-random BLE MAC format**: `mac[5] |= 0xC0` sets bits 47:46 to `11`, which is required
by the BLE spec for static random addresses. Always do this when generating a new MAC.

---

## Common Gotchas

### `bt_hid_remove_pairing` requires BLE to be active
This function calls `furi_hal_bt_start_advertising()`, which is undefined behaviour without an
active BLE profile. **Do not call it from the Settings scene or any scene entered from Settings**
— BLE is always stopped before entering Settings. Use `bt_remotes_profile_clear_pairing` instead.

### `bt_remotes_profile_activate` overwrites `.bt_hid.cfg`
When navigating Back from Settings, `settings_on_event` calls `bt_remotes_profile_activate`
before `bt_remotes_start_ble`. `profile_activate` copies the profile's `.cfg` → `.bt_hid.cfg`.
If you saved a name change only to `.bt_hid.cfg` (e.g. via `bt_hid_save_cfg`) but not to the
profile's `.cfg`, the name will be silently overwritten. **Always write name changes to the
profile's `.cfg` file immediately** — the rename scene does this with a `storage_common_copy`
after `bt_hid_save_cfg`.

### `on_exit` fires for both forward and backward navigation
You cannot reliably use `on_exit` to detect whether the user pressed Back vs. selected a menu
item. Use `on_event` with `SceneManagerEventTypeBack` instead.

### `profile_select` auto-advance guard
`profile_select_on_enter` checks `app->ble_started` and, if true, immediately sends
`BtRemotesProfileSelectEventAutoAdvance` to skip directly to the Start scene. This handles the
case where `profile_new` creates a profile and pops back. If you stop BLE and clear
`active_profile` before navigating to ProfileSelect (as the Start Back handler does), this guard
will be false and the full profile list will render.

### The rename scene is context-aware
`bt_remotes_scene_rename` behaves differently depending on `active_profile`:
- **No profile** → edits `app->default_ble_name`, saves to `app.cfg`. No BLE involved.
- **Profile active** → edits `app->ble_hid_cfg.name`, saves to `.bt_hid.cfg` AND copies to
  profile `.cfg`. BLE is already stopped; restart happens on Back from Settings.

---
