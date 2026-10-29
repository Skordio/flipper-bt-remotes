# bt_remotes — Architecture & Internals

Developer reference for the `bt_remotes` Flipper Zero app. For a user-facing overview see
[`../README.md`](../README.md); for the short list of rules/invariants an agent must follow see
[`../AGENTS.md`](../AGENTS.md). This document is the detailed technical source of truth.

`bt_remotes` is a BLE HID remote-control app: the Flipper acts as a Bluetooth HID peripheral
(keyboard, mouse, media, etc.) with multiple named profiles, each with its own BLE MAC address
and bonding keys. On top of the basic remotes it adds a reorderable/hideable/pinnable Start menu,
Ducky Scripts + Collections, a Custom Gestures scripting language, and per-remote settings.

---

## Firmware Compatibility (Momentum-targeted)

This app targets **Momentum firmware**. As packaged it will **not load** on stock OFW, Unleashed,
or RogueMaster — those firmwares reject it at load with **"Update Firmware to use with this
Application"** (`MissingImports`). There is **no runtime firmware guard**; the loader rejection is
the de facto gate.

**The real reason (verified, not the BLE story below).** The FAP imports several **app-API symbols
that only Momentum exports** to apps. Diffing the built FAP's undefined symbols against each
firmware's `targets/f7/api_symbols.csv` (`+` = exported to apps) found these blockers:

| Imported symbol(s) | Stock 1.4.3 | Unleashed | RogueMaster | Momentum |
|---|---|---|---|---|
| 16 built-in icons (`I_Ble_connected_15x15`, `I_ButtonUp_7x4`, `I_DolphinDone_80x58`, …) | ❌ | ❌ | ❌ | ✅ |
| `variable_item_list_set_header` | ❌ | ❌ | ✅ | ✅ |
| `strtok` (libc) | ❌ (`-`, not app-exported) | ✅ | ✅ | ✅ |
| `momentum_settings` (read as `momentum_settings.wrap_on_hold` for List Wraparound) | ❌ | ❌ | ❌ | ✅ |

Any one missing import makes the loader refuse the FAP. The **16 built-in icons and
`momentum_settings` are the universal blockers** across all three other firmwares.

`wrap_on_hold` itself exists only in this fork's Momentum (firmware commit `d18c70976` appends it at
the **end** of `MomentumSettings`, `lib/momentum/settings.h`). On upstream Momentum the app still
loads, but the `views/hid_remote_menu.c` / `views/hid_ptt_menu.c` read lands past the end of
upstream's struct, so List Wraparound behavior there is undefined; building against upstream
headers fails to compile.

**It is NOT a Bluetooth/`GapConfig` problem.** An earlier version of this doc claimed the app was
Momentum-only because `GapConfig.mac_address` (the per-profile custom BLE address) existed only in
Momentum. That was wrong: `GapConfig` (with `mac_address`) and `BleGattCharacteristicParams` are
**byte-identical** across Momentum, stock 1.4.3, Unleashed, and RogueMaster, and stock 1.4.3's
`gap.c` applies the custom address the same way. The custom-address feature works on all of them —
the app's BLE code (`helpers/ble_hid_ext_profile.c`) would be ABI-compatible. The block is purely
the exported app-API surface above.

**Path to broader compatibility (not done yet).** This is mostly accidental coupling and is
fixable:
- The **16 icons are already bundled** in this app's `assets/`. The code references the *firmware*
  built-ins (`&I_…`) instead of the app's own bundled copies — switch those references to the
  bundled icons.
- Replace **`strtok`** (used in profile-order parsing) with a small inline tokenizer.
- Avoid **`variable_item_list_set_header`** (drop the header or set it another way).
- Drop the **`momentum_settings.wrap_on_hold`** reads in `views/hid_remote_menu.c` and
  `views/hid_ptt_menu.c` (always wrap on a fresh press, or gate the read on a firmware that has it).

After those changes the app would likely load and run on stock / Unleashed / RogueMaster too (the
BLE ABI already matches), at which point the "Momentum-only" framing could be dropped. Until then,
treat Momentum as the supported target. To re-check compatibility after any change:
`arm-none-eabi-nm -u <built>.fap` → diff the undefined symbols against a firmware's
`api_symbols.csv` `+` entries.

---

## Repository Layout

```
bt_remotes/
├── bt_remotes.c              # Core logic: BLE lifecycle, profile/collection/
│                             #   gesture/pins config I/O, app alloc/free/entry
├── bt_remotes.h              # Hid struct, all constants, enums, all public function decls
├── hid.h                     # 1-line shim (#include "bt_remotes.h") so views/*.c can keep
│                             #   #include "../hid.h"
├── views.h                   # HidView enum (all view IDs used with view_dispatcher)
├── transport_ble.c           # hid_hal_* wrappers → ble_profile_hid_* (BLE HID transport)
├── application.fam           # Build metadata (EXTERNAL, entry bt_remotes_app, 4 KB stack,
│                             #   cdefines HID_TRANSPORT_BLE, fap_libs ble_profile)
├── helpers/
│   ├── ble_hid_ext_profile.c/h # BleProfileHidExtParams (name[20], mac[6]) + the custom
│   │                           #   FuriHalBleProfileTemplate that injects per-profile MAC/name
│   ├── ducky_runner.c/h        # DuckyScript parser + worker-thread runner (for Ducky Scripts)
│   └── gesture_runner.c/h      # Custom Gestures language: parser + worker-thread runner
├── scenes/
│   ├── bt_remotes_scenes.h     # ADD_SCENE macro list — one line per scene (source of truth)
│   ├── bt_remotes_scene.c/h    # Generated scene dispatch table
│   │   # Profile/identity:
│   ├── bt_remotes_scene_profile_select.c   bt_remotes_scene_profile_new.c
│   ├── bt_remotes_scene_profile_rename_file.c   bt_remotes_scene_rename.c
│   ├── bt_remotes_scene_save_profile.c   bt_remotes_scene_delete_profile.c
│   ├── bt_remotes_scene_reset_profile.c   bt_remotes_scene_unpair.c
│   │   # Navigation / menu:
│   ├── bt_remotes_scene_start.c   bt_remotes_scene_main.c
│   ├── bt_remotes_scene_global_settings.c   bt_remotes_scene_profile_settings.c
│   ├── bt_remotes_scene_profile_connection.c   bt_remotes_scene_profile_menu_layout.c
│   ├── bt_remotes_scene_profile_management.c   bt_remotes_scene_help.c
│   ├── bt_remotes_scene_profile_shortcut_create.c   bt_remotes_scene_reset_bt_name.c
│   ├── bt_remotes_scene_hide_items.c   bt_remotes_scene_reset_menu.c
│   │   # Per-remote settings:
│   ├── bt_remotes_scene_remote_type_settings.c   bt_remotes_scene_remote_settings_help.c
│   ├── bt_remotes_scene_keynote_settings.c   bt_remotes_scene_media_settings.c
│   ├── bt_remotes_scene_tiktok_settings.c   bt_remotes_scene_ios_phone_settings.c
│   ├── bt_remotes_scene_ducky_settings.c
│   │   # Ducky Scripts + Collections:
│   ├── bt_remotes_scene_custom_actions.c   bt_remotes_scene_custom_actions_run.c
│   ├── bt_remotes_scene_collection_list.c   bt_remotes_scene_collection_view.c
│   ├── bt_remotes_scene_collection_create.c   bt_remotes_scene_collection_edit.c
│   ├── bt_remotes_scene_collection_delete.c   bt_remotes_scene_collection_pin.c
│   │   # Custom Gestures:
│   ├── bt_remotes_scene_gesture_list.c   bt_remotes_scene_gesture_run.c
│   └── bt_remotes_scene_gesture_pin.c
└── views/
    ├── hid_remote_menu.c/h    # Reorderable/hideable/pinnable Start-menu view (BtRemotesMenuEntry)
    ├── hid_keyboard.c/h   hid_keynote.c/h   hid_numpad.c/h
    ├── hid_media.c/h   hid_music_macos.c/h   hid_movie.c/h   hid_tiktok.c/h
    ├── hid_ios_phone.c/h
    ├── hid_mouse.c/h   hid_mouse_clicker.c/h
    ├── hid_mouse_jiggler.c/h   hid_mouse_jiggler_stealth.c/h
    ├── hid_ptt.c/h   hid_ptt_menu.c/h
    └── hid_draw_shared.h    # shared inline arrow glyphs (hid_draw_arrow_dot/_line)
```

(Plus `assets/` — PNG icons for the remote views — `bt_remotes_10px.png`, the app icon, `README.md`,
`docs/` — this file, `WRITING_GESTURES.md`, and `agents/` (config for the agent engineering skills) — and `examples/`, sample `.gesture` files.)

The two custom list views (`hid_remote_menu` — also Profile Select — and the PTT menu's Up/Down
list) honor Momentum's global "List Wraparound" setting (`momentum_settings.wrap_on_hold`, see
**Firmware Compatibility**) at the list extremes, same as the firmware's
submenu/variable_item_list. PTT Left/Right tab cycling is unaffected.

---

## Key Constants (`bt_remotes.h`)

| Constant | Value | Purpose |
|---|---|---|
| `BT_REMOTES_PROFILES_DIR` | `APP_DATA_PATH("profiles")` | Profile + `.pins` storage directory |
| `BT_REMOTES_APP_CFG_PATH` | `APP_DATA_PATH("app.cfg")` | App-level config (default name, vibro, Back-hold, profile order) |
| `BT_REMOTES_CFG_EXT` / `_KEYS_EXT` | `.cfg` / `.keys` | Per-profile config / bonding-keys extensions |
| `BT_REMOTES_PROFILE_NAME_LEN` | `32` | Max profile name length (including NUL) |
| `BT_REMOTES_PROFILE_MAX_COUNT` | `16` | Max number of profiles |
| `BT_REMOTES_MENU_ITEM_COUNT` | `17` | Number of fixed Start-menu items (enum `BtRemotesStartIndex*`) |
| `BT_REMOTES_PINNED_MAX` | `16` | Max pinned collections/gestures on the Start menu |
| `BT_REMOTES_MENU_ORDER_LEN` | `33` | `MENU_ITEM_COUNT + PINNED_MAX` — length of `menu_order[]` |
| `BT_REMOTES_MENU_ITEM_COUNT_V1` / `_ORDER_LEN_V1` | `15` / `31` | Pre-Custom-Gestures layout, for migrating saved `menu_order` in legacy cfgs (no `menu_item_count` key) |
| `BT_REMOTES_MENU_ITEM_COUNT_V2` / `_ORDER_LEN_V2` | `16` / `32` | Post-Custom-Gestures, pre-iOS-Phone layout, same legacy-only migration |
| `BT_REMOTES_COLLECTION_DIR` | `APP_DATA_PATH("collections")` | Ducky Script Collections directory |
| `BT_REMOTES_COLLECTION_EXT` | `.collection` | Collection file extension |
| `BT_REMOTES_COLLECTION_MAX` / `_SCRIPT_MAX` | `16` / `16` | Max collections / scripts-per-collection |
| `BT_REMOTES_LAUNCHER_DIR` / `_EXT` | `APP_DATA_PATH("launchers")` / `.btremote` | Profile-launcher shortcut directory / extension |
| `BT_REMOTES_GESTURE_DIR` | `APP_DATA_PATH("gestures")` | Custom Gestures library directory (global) |
| `BT_REMOTES_GESTURE_EXT` | `.gesture` | Gesture file extension |
| `BT_REMOTES_GESTURE_MAX` / `_NAME_LEN` | `32` / `32` | Max gestures / gesture-name length |
| `GESTURE_LINE_LEN` | `64` | Max chars per gesture line, incl. NUL (in `gesture_runner.h`); no max line count — the runner streams the file |
| `BT_REMOTES_BACK_HOLD_EXIT_S_MIN` / `_MAX` / `_DEFAULT` | `1` / `10` / `6` | Hold-Back-to-Quit seconds range / default |
| `HID_BT_KEYS_STORAGE_NAME` | `.bt_hid.keys` | Active bonding keys filename |
| `FURI_HAL_BT_ADV_NAME_LENGTH` | `20` | Max BLE device name length (from furi_hal_version.h) |

---

## The `Hid` Struct (abridged — see `bt_remotes.h` for the full definition)

The struct is large; these are the fields agents touch most. Names/types are exact.

```c
struct Hid {
    // BLE
    FuriHalBleProfileBase* ble_hid_profile;   // live profile handle (NULL when stopped)
    BleProfileHidExtParams ble_hid_cfg;       // .name[20], .mac[6] — loaded by profile_activate
    bool ble_started;                          // true while HID profile is running
    bool connected;                            // live link state (status callback; cleared by stop_ble)
    Bt* bt; Gui* gui; NotificationApp* notifications; Storage* storage;

    // GUI infra + view modules
    ViewDispatcher* view_dispatcher; SceneManager* scene_manager;
    Submenu* submenu; VariableItemList* var_item_list; DialogEx* dialog;
    TextInput* text_input; Popup* popup; Widget* help_widget;
    HidRemoteMenu* hid_remote_menu;            // the Start menu
    // Remote views are lazy: NULL until first navigation (bt_remotes_view_ensure);
    // so are file_browser and hid_ptt/hid_ptt_menu. Only hid_remote_menu is eager.
    HidKeynote* hid_keynote; HidKeyboard* hid_keyboard; /* ...all remote views... */

    // Collections + pins (Ducky Scripts)
    char    collection_names[16][32];   uint8_t collection_count;
    char    pinned_collections[16][32]; uint8_t pinned_kinds[16];  // 0=collection, 1=gesture
    uint8_t pinned_count;
    char    editing_collection_name[32];
    // 4 KB heap scratch: alloc'd on demand (bt_remotes_collection_scripts_ensure,
    // called by collection_load/collection_create), freed on return to the Start
    // menu (bt_remotes_collection_scripts_release). NULL while released.
    char    (*editing_collection_scripts)[256]; uint8_t editing_collection_script_count;

    // Custom Gestures (global library + editing buffer)
    char    gesture_names[32][32]; uint8_t gesture_count;
    char    editing_gesture_name[32];

    // Profile management
    char active_profile[32];   // filename stem of the active profile; "" = none selected
    char pending_name[32];     // old profile name held during a rename operation
    char default_ble_name[20]; // default BT name applied when creating new profiles
    char profile_list[16][32]; uint8_t profile_count;
    uint8_t pending_launcher_remote;  // .btremote deep-link target; BT_REMOTES_LAUNCHER_REMOTE_NONE (0xFF) = none

    // Per-profile settings (each one row in bt_remotes_profile_settings[])
    uint8_t keynote_back_key, media_mode, media_mouse_switch, tiktok_scroll_mode;
    uint16_t tiktok_gesture_inset, tiktok_gesture_margin, tiktok_gesture_swipe;
    /* also ios_* (7 fields), delay_connect, ducky_connect_per_run, ducky_connect_settle_ms */

    // App-level + menu layout
    uint8_t  vibro_mode;                          // 0=Neither 1=Disconnect 2=Connect 3=Both
    uint8_t  back_hold_exit_s;                    // seconds Back must be held to quit the app
    uint8_t  menu_order[BT_REMOTES_MENU_ORDER_LEN]; // 0xFF = unused slot; >=MENU_ITEM_COUNT = pinned slot
    uint32_t menu_hidden;                          // bitmask: bit i set → item i hidden
    char     profile_order_str[...];               // pipe-separated saved profile order

    // Ducky Scripts file browser + runners
    FileBrowser* file_browser; FuriString* file_browser_result;
    DuckyRunner* ducky_runner; GestureRunner* gesture_runner;
    char pending_script_path[256];

    // Post-pairing auto-save timer
    FuriTimer* pair_save_timer; uint8_t pair_save_attempts;

    // Hold-Back-to-Quit watchdog + Ducky connect-per-run wait
    FuriTimer* back_hold_timer; FuriPubSub* input_events; FuriPubSubSubscription* input_subscription;
    FuriTimer* connect_wait_timer; uint8_t connect_wait_attempts, connect_settle_ticks;
    bool connect_wait_active;
};
```

---

## On-Disk File Formats

All config files are FlipperFormat text. Constants/file-types live in `bt_remotes.c` and
`gesture_runner.h`.

### Per-profile files — `APP_DATA_PATH("profiles/")`

- **`{name}.cfg`** — `Flipper BT Remote Settings File` v1. Holds the BLE identity, the per-profile
  menu layout, **and** the per-remote settings:
  ```
  Filetype: Flipper BT Remote Settings File
  Version: 1
  name: My Keyboard
  mac: C0 AB CD EF 12 34
  menu_order: <33 uint32 values>     # fixed idx 0‥16, pinned slot 17‥32, 0xFF = unused
  menu_item_count: <uint32>          # fixed-item count menu_order was saved with (generic migration)
  menu_hidden: <uint32 bitmask>      # bit i set → fixed item i hidden
  keynote_back_key: <uint32>         # KeynoteBackKey enum
  media_mode: <uint32>               # MediaMode enum
  media_mouse_switch: <uint32>       # 0/1
  tiktok_scroll_mode: <uint32>       # TikTokScrollMode enum
  tiktok_gesture_inset / _margin / _swipe: <uint32>   # px tunables
  ios_cursor_mode: <uint32>          # iOS Phone hold movement: 0=Constant 1=Ramp
  ios_cursor_speed_px_s: <uint32>    # iOS Phone hold speed (px/sec; ramp top speed)
  ios_ramp_start_pct: <uint32>       # iOS Phone ramp start (% of cursor speed)
  ios_ramp_time_ms: <uint32>         # iOS Phone ramp climb time (ms)
  ios_swipe_distance: <uint32>       # iOS Phone drag distance (px)
  ios_swipe_speed_px_s: <uint32>     # iOS Phone drag speed (px/sec)
  ios_dbl_tap_window_ms: <uint32>    # iOS Phone double-tap window (ms)
  ios_dbl_tap_swipe: <uint32>        # 0/1 — default-mode d-pad double-tap fires a swipe
  delay_connect: <uint32>            # 0/1 — defer BLE start until a remote is opened
  ducky_connect_per_run: <uint32>    # 0/1 — Ducky/Collections connect only during a run
  ducky_connect_settle_ms: <uint32>  # ms to wait after link-up before sending HID (per-run)
  ```
  `bt_remotes_save_profile_menu_cfg` writes the **full** set above and is the only writer of the
  profile `.cfg` — the single on-disk source of truth for the BLE identity. Callers that change
  `name`/`mac` update `app->ble_hid_cfg` in memory first, then call `save_profile_menu_cfg`.
  `bt_remotes_profile_create` writes only `name` + `mac`; every other key then loads its default.
  Files missing any key load with safe defaults (enum-style rows snap an out-of-range value to the
  default, range rows clamp). `menu_order` is migrated on read in `bt_remotes_profile_activate`:
  files carrying `menu_item_count` migrate generically (pinned-slot values shift up by the number
  of fixed items added since the save); older files fall through the legacy length-keyed arms
  (33-entry, V2, V1, fixed-only).
- **`{name}.keys`** — BLE bonding keys binary blob (copied from the active `.bt_hid.keys` by
  `bt_remotes_profile_save`).
- **`{name}.pins`** — `Flipper BT Collection Pins` v1. Per-profile list of Start-menu pins:
  ```
  Filetype: Flipper BT Collection Pins
  Version: 1
  count: 2
  collection_0: My Macros
  kind_0: 0                      # 0 = collection
  collection_1: Head Track
  kind_1: 1                      # 1 = gesture
  ```
  Older `.pins` files omit `kind_N` and default to collection (kind 0). `profile_rename` moves the
  `.pins` file with the profile and `profile_delete` removes it.

The **active** profile's keys are mirrored to `APP_DATA_PATH(".bt_hid.keys")` — that file is what
the BT stack reads at runtime. The BLE identity (name + MAC) has no on-disk mirror: it lives in
`app->ble_hid_cfg`, populated by `bt_remotes_profile_activate` from the profile `.cfg` and passed
to `bt_profile_start` directly.

### App-level config — `APP_DATA_PATH("app.cfg")`

`Flipper BT Remotes App Config` v1 — settings shared across all profiles:
```
Filetype: Flipper BT Remotes App Config
Version: 1
default_name: Flipper Zero       # BT name new profiles inherit ("" if unset)
vibro_mode: 1                    # 0=Neither 1=Disconnect 2=Connect 3=Both (default 1)
back_hold_exit_s: 6              # seconds Back must be held to quit the app (1-10, default 6)
profile_order: Keyboard|Mouse|…  # pipe-separated saved profile display order
```
Missing keys fall back to defaults (default_name "", vibro 1, back-hold 6 s, no profile order).
Legacy files with the old `disconnect_vibro` bool simply fail the uint32 read and default to 1.

### Collections (Ducky Scripts) — `APP_DATA_PATH("collections/")`

- **`{name}.collection`** — `Flipper BT Ducky Collection` v1: `count` + `script_0`, `script_1`, …
  (each a full path to a `.txt` DuckyScript). Up to `BT_REMOTES_COLLECTION_SCRIPT_MAX` (16).

### Custom Gestures (global) — `APP_DATA_PATH("gestures/")`

- **`{name}.gesture`** — plain text (computer-editable): **one command per line** (blank lines and
  `#` comments allowed). **No header required**; a legacy `Filetype:`/`Version:` header parses as
  invalid lines, which the runner skips, so older files keep loading. No `count` / `line_N` keys —
  not FlipperFormat. The app never writes gestures (no on-device editor): files are authored on a
  computer and dropped on the SD card; the app only lists them (`bt_remotes_gesture_load_list`) and
  the runner streams them (`gesture_run_file`). Global = shared across all profiles. See
  **Custom Gestures** and [`WRITING_GESTURES.md`](WRITING_GESTURES.md).

### Profile Launcher shortcuts — `APP_DATA_PATH("launchers/")`

- **`{name}.btremote`** — `BT Remotes Launcher` v1: `Profile` field naming the profile file's
  basename (no path, no `.cfg`), plus at most one optional target field:
  - `Remote` — a fixed Start-menu item by its `bt_remotes_menu_default[].label` (e.g. `Media`,
    `TikTok / YT Shorts`, `Ducky Scripts`; Settings excluded). Label-based reference survives
    menu reorder/hiding and future enum-index shifts.
  - `Pin` — a currently-pinned Ducky collection or Custom Gesture by name. Resolved against the
    profile's pins at launch; if the item has been unpinned, the launch falls back to the Start
    menu (a `FURI_LOG_W` notes it). If both fields are present, `Remote` wins.
  Optional-field reads each `flipper_format_rewind` first — a failed key search leaves the
  cursor at EOF, which would otherwise swallow the next read.
- Opened from the Flipper's File Browser (or Favorites) → firmware routes the extension to the
  `bt_remotes.fap` and hands the path in via the entry-point argument. Extension registration lives
  in the firmware archive (`applications/main/archive/`): `ArchiveFileTypeBtRemotesLauncher` in
  `helpers/archive_files.h`, `.btremote` in `helpers/archive_browser.h`'s `known_ext[]`, and the
  `EXT_PATH("apps/Bluetooth/bt_remotes.fap")` mapping in `scenes/archive_scene_browser.c`.
- `bt_remotes_launcher_try_load` (in `bt_remotes.c`) opens the file, validates filetype+version,
  reads `Profile` (rejecting names that fail `bt_remotes_validate_name` or exceed
  `BT_REMOTES_PROFILE_NAME_LEN`), checks the matching `.cfg` exists, sets `app->active_profile`,
  and calls `bt_remotes_profile_activate`. On any failure the app state is left clean and the
  entry point falls through to Profile Select (with a `FURI_LOG_W` line for diagnosis).
- Launcher-launched sessions push Profile Select underneath Start so Back from Start still lands on
  Profile Select (matching the normal in-app flow).
- **Target deep-link (`Remote` / `Pin` fields):** on a match, `try_load` sets
  `app->pending_launcher_remote` (sentinel `BT_REMOTES_LAUNCHER_REMOTE_NONE`) — a fixed index
  for `Remote`, or `BT_REMOTES_MENU_ITEM_COUNT + pidx` for a `Pin` resolved against the pins
  that `bt_remotes_pinned_load` loaded moments earlier. The Start scene's `on_enter` fires the
  value as a custom event **after** resetting the field (so Back-from-remote re-entry doesn't
  re-fire), and the event lands in Start's own `on_event` — reusing the normal routing verbatim:
  delay-connect BLE start, index→view_id mapping, pinned-slot kind dispatch (collection view vs
  gesture run), and stack shape (ProfileSelect → Start → destination). It is deliberately NOT
  queued from the entry point: Profile Select consumes every custom event and its event enum
  overlaps remote indices 0–4. Unknown targets don't fail the launch — the profile opens to
  Start with a `FURI_LOG_W`.
- Written by the **Create Shortcut** row under Profile Settings → Profile Management: a submenu
  picker ("Profile Only" + every fixed item except Settings + the profile's pinned
  collections/gestures), then the file is written to `BT_REMOTES_LAUNCHER_DIR` (created on first
  write, not at app start; overwrites if present). Profile-only shortcuts are `{profile}.btremote`;
  target shortcuts are `{profile} - {name}.btremote` with filesystem-hostile chars in the name
  replaced by `-` ("TikTok / YT Shorts" contains `/`).
  Users can move the file anywhere on the SD card — routing is by extension, not location.
  Renaming a profile after creating a shortcut breaks it (documented in the Profile Management
  help page).

---

## BLE Lifecycle

### `bt_remotes_start_ble(Hid* app)`

1. `furi_assert(!app->ble_started)` — **must not** be called while already started
2. Resets the blue LED off
3. Sets the BT keys storage path to `APP_DATA_PATH(".bt_hid.keys")`
4. Registers `bt_remotes_connection_status_changed_callback`
5. Starts the HID profile with `bt_profile_start(app->bt, ble_profile_hid_ext, &app->ble_hid_cfg)`
   and `furi_check`s the handle — `ble_hid_cfg` (name + MAC) was populated by
   `bt_remotes_profile_activate`; it is all-zero (profile defaults) when no profile is active
6. Starts advertising; sets `app->ble_started = true`

**Registers the status callback every time it is called** — the callback would otherwise be lost
after any stop/start cycle. Note it **asserts** `!ble_started` (it does not guard); callers must
stop BLE first. `ble_profile_hid_ext` is this app's own profile template
(`helpers/ble_hid_ext_profile.c`) that injects the per-profile MAC/name.

### `bt_remotes_stop_ble(Hid* app)`

Has an `if(!app->ble_started) return;` guard — safe to call redundantly. Then:

1. Stops the pairing-save timer (`furi_timer_stop(app->pair_save_timer)`)
2. Clears the status callback (`bt_set_status_changed_callback(NULL, NULL)`)
3. Resets the blue LED off
4. Disconnects (`bt_disconnect`); waits 200 ms only if `app->connected` (nothing to flush otherwise)
5. Resets keys path to default (`bt_keys_storage_set_default_path`)
6. Restores default BT profile (`furi_check(bt_profile_restore_default)`)
7. Sets `app->ble_started = false`, NULLs `app->ble_hid_profile` (the `hid_hal_*` wrappers then
   no-op stale input events) and clears `app->connected` (the callback is already detached)

### Connection Status Callback

```c
static void bt_remotes_connection_status_changed_callback(BtStatus status, void* context)
```

Fires on every BT status change, on the BT service thread (so no SD I/O). It:
- Sets `app->connected` and the blue LED (on connect / off otherwise)
- Fires a vibro pulse per `vibro_mode` (connect if mode 2/3, disconnect if mode 1/3)
- Posts `BT_REMOTES_EVENT_CONNECT_TICK` on link-up (for the Ducky connect-per-run wait)
- Updates connected state in every remote view that has been built (views are lazy, so each
  pointer is NULL-checked)

**Pairing auto-save (the important part).** When a profile is active and the status becomes
connected, the callback only resets `pair_save_attempts` and starts `pair_save_timer` (period
`PAIR_SAVE_POLL_MS` = 200 ms). The timer callback just posts `BT_REMOTES_EVENT_PAIR_SAVE_TICK`;
`bt_remotes_custom_event_callback` handles it on the dispatcher thread (it never reaches the
scenes) by calling `bt_remotes_profile_save`, which returns false until `.bt_hid.keys` exists. The
poll stops on the first successful save or after `PAIR_SAVE_MAX_ATTEMPTS` = 25 (≈ 5 s):
- **Reconnect** (keys already on disk, restored by `profile_activate`) → the first tick saves,
  capturing any refreshed key material.
- **First-time pairing** → the BLE link connects *before* SMP bonding finishes, so `.bt_hid.keys`
  doesn't exist yet; the poll saves as soon as the stack writes it.

On **disconnect** it stops the pairing-save timer. (This polling approach replaced an earlier
assumption that the BT service always writes keys before firing the status callback.)

### Deferred BLE start (`delay_connect`, per profile)

By default a profile starts BLE on selection (`profile_select`) and stays advertising the whole
session. When the per-profile `delay_connect` flag is set, BLE is instead brought up **only while a
remote/script/gesture screen is open**, and dropped on return to the Start menu:

- **`profile_select`** skips `start_ble` when `delay_connect` is set (lands on Start disconnected).
- **`start_on_event`** calls `start_ble` (guarded by `!ble_started`) for *any* selection except
  Settings — covering remote views, the Ducky/Gesture lists, and pinned-slot launches in one place.
- **`start_on_enter`** calls `stop_ble` when `delay_connect` is set, so every return to the menu
  disconnects. (Entering a remote pushes `Main` over `Start`, so `start_on_enter` doesn't re-run
  until Back — no start/stop churn.)
- The `unpair` / `reset_profile` / `scene_rename` / `reset_bt_name` restarts are all gated by
  `!delay_connect` (they call `bt_remotes_start_ble_if_immediate`), so management flows stay
  disconnected at the menu in delay mode.

`delay_connect` is **off by default**. New profiles get the default via the
`bt_remotes_profile_activate(app)` call the `profile_new` flow now runs after `profile_create` —
that also defaults *all* the other per-profile fields (menu layout + per-remote settings) instead
of inheriting the previous profile's stale in-memory values. Toggle lives in Settings → Connection →
"Delay Connect" (`profile_connection`; saved immediately). The immediate-vs-delay start sites
(`profile_select`, `unpair`, `reset_profile`, `scene_rename`, `reset_bt_name`, and the `.btremote`
launcher entry) go through the `bt_remotes_start_ble_if_immediate(app)` helper.

### Ducky/Collections "connect per run" (`ducky_connect_per_run`, per profile)

An **independent** per-profile policy that narrows the connection window for Ducky Scripts and
Collections to a single script's execution (Custom Gestures are unaffected). When set, regardless
of `delay_connect`:

- **Browsing is disconnected.** `custom_actions` and `collection_view` `on_enter` call
  `bt_remotes_ducky_browse_enter`, which `stop_ble`s when the flag is set (idempotent; this is what
  disconnects an immediate-mode profile). `start_on_event` also skips its
  `delay_connect` BLE-start for the Ducky list and pinned **collection** (kind 0) launches so there
  is no start/stop churn.
- **The run scene connects just for the run.** `custom_actions_run` `on_enter`: if not already
  `connected`, `start_ble` and show "Connecting…", then poll `app->connected` via
  `connect_wait_timer` (period `CONNECT_WAIT_POLL_MS`, cap `CONNECT_WAIT_MAX_ATTEMPTS` ≈ 15 s,
  posting `BT_REMOTES_EVENT_CONNECT_TICK`, which the status callback also posts on link-up; the
  scene ignores ticks unless `connect_wait_active`). On connect, **settle** `ducky_connect_settle_ms`
  (the BLE link comes up before the host has subscribed to HID report notifications — sending
  immediately drops the first keystrokes; the settle counts whole `CONNECT_WAIT_POLL_MS` ticks)
  then start the runner; on timeout → "Not Connected"; Back cancels. `on_exit` always `stop_ble`
  in this mode (covers Done/Error/cancel).
- **The menu restores the profile's normal state.** `start_on_enter` does
  `if(delay_connect) stop_ble; else if(!ble_started) start_ble;` — the `else if` reconnects an
  immediate-mode profile after a per-run session dropped the link. A remote opened after a Ducky
  run always passes through Start first, so it finds BLE restored.

`app->connected` (set in the status callback, force-cleared in `stop_ble` since the callback is
detached before the disconnect fires) is the gate the run scene waits on. The "Connect Per Run"
toggle and the "Connect Delay" (`ducky_connect_settle_ms`) tunable live together under
Settings → Per-Remote Settings → **DuckyScript** (`scene_ducky_settings.c`, a `VariableItemList`
modeled on the TikTok settings scene). Off by default.

---

## Start Menu (`hid_remote_menu` + `menu_order` / `menu_hidden` / pins)

The Start screen is a custom reorderable list, not a submenu. Item identity is the
`BtRemotesStartIndex` enum (`0..BT_REMOTES_MENU_ITEM_COUNT-1`, Settings always last at the
highest index). Normal-mode navigation wraps at the extremes per Momentum's global
"List Wraparound" setting (`momentum_settings.wrap_on_hold`: Instant wraps even mid-hold;
Re-press clamps during a hold and wraps only on a fresh press), always within the *visible*
(divider-limited) range; reorder mode never wraps. `bt_remotes_menu_default[]` (in `bt_remotes_scene_start.c`) maps each index to a
label and is kept in **enum order** so `bt_remotes_menu_default[i].index == i` — `hide_items` and
several guards rely on this.

- **`menu_order[BT_REMOTES_MENU_ORDER_LEN]`** (per profile): visual order. Values
  `0..MENU_ITEM_COUNT-1` = fixed items, `MENU_ITEM_COUNT..ORDER_LEN-1` = pinned slot
  `value-MENU_ITEM_COUNT` into `pinned_collections[]`, `0xFF` = unused slot.
- **`menu_hidden`** (per profile): bitmask, bit `i` set → fixed item `i` hidden.
- **Pins**: `pinned_collections[i]` + `pinned_kinds[i]` (0 = collection, 1 = gesture), persisted
  per profile in `{name}.pins`. A pinned slot launches `CollectionView` or `GestureRun` by kind.
  Add/remove pins only through `bt_remotes_pin_add` / `bt_remotes_pin_remove`: because
  `menu_order` references pins positionally, `pin_remove` also remaps the slots (removed pin →
  `0xFF`, later pins shift down one) and persists both files — hand-shifting the arrays would
  silently repoint every later `menu_order` slot at the wrong pin.

`start_on_enter` rebuilds the displayed list from `menu_order`, then **appends** (a) any fixed
item missing from a saved order — so items added in a firmware update (e.g. Custom Gestures, iOS
Phone) appear on existing profiles without a "Reset Menu Order", and (b) any newly-pinned
collection not yet in `menu_order`. **Settings must stay the highest-indexed fixed item** —
`hide_items` and the load-time "never hide Settings" guard depend on it. When inserting a new
fixed item, add it *before* Settings and bump `BT_REMOTES_MENU_ITEM_COUNT` (and the
`REMOTE_MENU_MAX_ITEMS` capacity in `hid_remote_menu.h`, which a `_Static_assert` in `scene_start.c`
checks) — no new migration arm is needed. Profile `.cfg`s carry `menu_item_count`, so
`bt_remotes_profile_activate` shifts older saved pinned-slot values up generically (the old Settings
value is reinterpreted as the first new item and the real Settings re-appends via `start_on_enter`).
The V1 / V2 / 33-entry arms only serve files saved before that key existed — don't extend them.

**Reset Menu Order** (Profile Settings → Start Menu Layout) restores identity order, sets every
pin slot to `0xFF` (pins stay in `.pins`; `start_on_enter` re-appends them after the fixed items)
and clears `menu_hidden`.

---

## Scene Navigation Map

```
ProfileSelect ──► ProfileNew ──► (back to ProfileSelect, auto-advance to Start)
    │
    ├──► Start (hid_remote_menu: reorderable/hideable items + pinned slots) ◄──────┐
    │      │  [Back] stops BLE, clears active_profile, returns to ProfileSelect    │
    │      │                                                                        │
    │      ├──► Main (remote views: Keynote[/Vertical], Keyboard, Numpad, Media,   │
    │      │      Apple Music macOS, Movie, TikTok, iOS Phone, Mouse[/Clicker/     │
    │      │      Jiggler/Stealth], PushToTalk→PTT menu). [Back from Main → Start] │
    │      │                                                                        │
    │      ├──► CustomActions ("Ducky Scripts") ─► Browse Files (file_browser)     │
    │      │         │                                 └─► CustomActionsRun         │
    │      │         └─► CollectionList ─► CollectionView/Create/Edit/Delete/Pin    │
    │      │              (View ─► CustomActionsRun; Create ─► Edit)               │
    │      │                                                                        │
    │      ├──► GestureList ("Custom Gestures", read-only) ─► GestureRun/Pin       │
    │      │                                                                        │
    │      ├──► (pinned slot) ─► CollectionView (kind 0) or GestureRun (kind 1)    │
    │      │                                                                        │
    │      └──► ProfileSettings (hub) ─┬──► ProfileConnection ─► Rename (Bluetooth   │
    │                                  │     Name), ResetBtName, SaveProfile (Save BT│
    │                                  │     Keys), Unpair, [Delay Connect: inline   │
    │                                  │     toggle], Help (RemoteSettingsHelp)      │
    │                                  ├──► ProfileMenuLayout ─► HideItems/ResetMenu/ │
    │                                  │     Help (RemoteSettingsHelp)               │
    │                                  ├──► RemoteTypeSettings ─► Keynote/Media/     │
    │                                  │     TikTok/Ducky/IosPhone Settings, Help    │
    │                                  │     (each Settings ─► RemoteSettingsHelp)   │
    │                                  └──► ProfileManagement ─► ProfileRenameFile,  │
    │                                        ProfileShortcutCreate, DeleteProfile,   │
    │                                        Help (RemoteSettingsHelp)               │
    │
    ├──► Help ("? Help" row: topic list → scrollable pages; Back → ProfileSelect)
    │
    └──► GlobalSettings (VariableItemList, from ProfileSelect, no active profile)
            ├──► Rename (Default BT Name → app->default_ble_name)
            ├──► (L/R row) Vibration
            ├──► (L/R row) Hold Back Quit (1-10 s)
            └──► Help (RemoteSettingsHelp / Global)
```

`bt_remotes_scenes.h` is the authoritative scene list. Start does **not** use a plain submenu — it
uses the custom `hid_remote_menu` view (see **Start Menu**). Routing of the selected index lives
in `bt_remotes_scene_start_on_event`. The `ResetProfile` scene is registered but **unreachable**
from the UI (nothing navigates to it; Unpair performs the same reset).

**Critical navigation rule**: BLE stays up through the whole Profile Settings sub-tree in the
default (immediate-connect) mode. `start_on_event` no longer calls `stop_ble` for the Settings
selection, and `profile_settings_on_event` no longer restarts BLE on Back — sub-scenes inherit a
live BLE session and the host doesn't drop. Three Settings actions need to re-advertise — the
per-profile **Bluetooth Name** rename (`scene_rename.c`), **Reset Name to Default**
(`scene_reset_bt_name.c`) and **Bluetooth Unpairing** (`scene_unpair.c`, which resets the MAC and
pairing): each performs a local `stop_ble` → (`profile_reset` for Unpair) → `profile_activate` →
`start_ble_if_immediate` after persisting the change (the `_if_immediate` keeps `delay_connect`
profiles offline). Ducky Scripts / Custom Gestures /
pinned-slot launches don't stop BLE either. In `delay_connect` mode the Start scene's
`on_enter`/`on_event` own the stop-at-menu / start-into-remote transitions; the Start scene's
`delay_connect` BLE-start branch already excludes the Settings selection, so delay-connect profiles
stay offline through Settings as before. `GlobalSettings` has no profile and never touches BLE.

**Hold-Back-to-Quit watchdog**: independent of scene navigation, `bt_remotes.c` subscribes to the
raw input pubsub (`RECORD_INPUT_EVENTS`) — Back **press** arms a one-shot `back_hold_timer` for
`back_hold_exit_s` seconds (app-level setting, 1–10 s, default 6), Back **release** disarms it.
Because the pubsub delivers every press/release regardless of view focus or event consumption, the
gesture works everywhere (remotes, text inputs, running scripts). When the timer fires it posts
`BT_REMOTES_EVENT_HOLD_EXIT`; `bt_remotes_custom_event_callback` handles it globally with
`scene_manager_stop` (so the current scene's `on_exit` cleanup runs) + `view_dispatcher_stop`,
then the normal teardown in `bt_remotes_free` stops BLE and frees everything. One wrinkle: after
stop, `view_dispatcher_run` blocks until all ongoing input is released — since Back is still
physically held at the deadline, the handler also publishes a synthetic Back `InputTypeRelease`
(software sequence source, same pubsub path as the `input send` CLI) so the app exits immediately;
the later real Release is discarded as non-complementary. The quick long-press Back that exits a
remote to the Start menu is a separate, unchanged mechanism.

---

## Ducky Scripts & Collections

Reached from the Start menu item **"Ducky Scripts"** (`BtRemotesStartIndexCustomActions` →
`bt_remotes_scene_custom_actions`). That scene is a 2-item hub:

- **Browse Files** → `file_browser` over `.txt` DuckyScript files; selecting one stores its path
  in `pending_script_path` and pushes `CustomActionsRun`, which executes it via **`ducky_runner`**
  (`helpers/ducky_runner.c` — a DuckyScript parser + worker-thread runner against the BLE HID
  profile; interruptible, with a done/error popup).
- **Collections** → `CollectionList`: named groups of script paths stored in
  `collections/{name}.collection`. Scenes: `collection_create` (name + validate, then pushes
  `collection_edit`), `collection_edit` (add/remove scripts, max 16), `collection_view` (lists the
  collection's scripts; selecting one pushes `CustomActionsRun` for it), `collection_delete`,
  `collection_pin` (pin to the Start menu, kind 0).

`ducky_runner` and the gesture runner share only the **threading/stop/popup pattern**, not code.

---

## Custom Gestures

A brand-new minimal mouse/keyboard scripting language with **its own** parser + runner
(`helpers/gesture_runner.c/.h`) — deliberately *not* built on `ducky_runner`. Reached from the
Start menu item **"Custom Gestures"** (`BtRemotesStartIndexCustomGestures` → `gesture_list`). The
library is **global** (`gestures/*.gesture`, shared across profiles); gestures can be pinned to the
Start menu (kind 1) and run from a pin or from the list.

> Authoring guide (for users writing gestures by hand): [`WRITING_GESTURES.md`](WRITING_GESTURES.md).
> The section below is the implementation reference.

**Language** (one command per line; `#`/blank lines ignored; verbs case-insensitive):

| Command | Meaning |
|---|---|
| `anchor tl\|tr\|bl\|br` | Slam the cursor to a screen corner (repeatable origin) |
| `move <dx> <dy>` | Relative mouse move (px), auto-chunked into int8 steps |
| `tap` | Finger tap = left press + release |
| `click left\|right` | Mouse button click |
| `drag <dx> <dy>` | Left press → stepped move → release |
| `press [left\|right]` / `release [left\|right]` | Hold / release a mouse button (default left) for manual drag timing |
| `scroll <n>` | Mouse wheel (signed) |
| `wait <ms>` | Delay (interruptible) |
| `key <combo>` | Keyboard key/combo, e.g. `key enter`, `key cmd space` |
| `type <text>` | Type a literal string |
| `run <name>` | **Script inheritance** — run another gesture by name |

**Authoring** is off-device: the on-device editor was removed (`ee6746b`), so gestures are
plain-text files written on a computer. `gesture_list` is a read-only browser of
`gestures/*.gesture`; picking one offers Run / Pin to Start. There is no line validation at
authoring time — the runner silently skips unknown or malformed lines. `gesture_run` executes via
`gesture_runner` with a Running/Done/Error popup; Back stops the run.

**Inheritance** (`run <name>`): `gesture_runner` executes recursively. `gesture_run_file` resolves
`<name>` as a **sibling** in the same `gestures/` dir, capped at `GESTURE_MAX_DEPTH` (5) to prevent
cycles; a missing child in a nested `run` is skipped (only a missing **top-level** file is an
error). The worker thread uses a 4 KB stack to accommodate the recursion. File format is plain
text, one command per line (see **On-Disk File Formats**).

---

## Per-Remote Settings (per profile)

Reached from **Profile Settings → Per-Remote Settings** (`remote_type_settings` hub). Five
sub-scenes, each a `VariableItemList`, all persisted in the profile `.cfg`:

- `keynote_settings` — `keynote_back_key` (`KeynoteBackKey`: Delete/Left/Escape/None).
- `media_settings` — `media_mode` (`MediaMode`: Legacy/Improved) + `media_mouse_switch` (short Back
  opens the mouse sub-view).
- `tiktok_settings` — `tiktok_scroll_mode` (Wheel/Gesture) + three px tunables
  `tiktok_gesture_inset` / `_margin` / `_swipe` (ranges/defaults defined in `bt_remotes.h`).
  The three tunables are only meaningful in Gesture mode and are **hidden** in Wheel mode — the
  Scroll Mode change callback posts a rebuild event back to the scene so the list can safely
  `variable_item_list_reset` outside the callback that's processing the change.
- `ios_phone_settings` — `ios_cursor_mode` (Constant/Ramp) + cursor/ramp/swipe/double-tap
  tunables (`ios_cursor_speed_px_s`, `ios_ramp_start_pct`, `ios_ramp_time_ms`,
  `ios_swipe_distance`, `ios_swipe_speed_px_s`, `ios_dbl_tap_window_ms`, `ios_dbl_tap_swipe`).
- `ducky_settings` — `ducky_connect_per_run` toggle + `ducky_connect_settle_ms` slider.

`remote_settings_help` is a shared help scene; the launching settings scene stores the
`RemoteSettingsHelpTopic` as its scene state before pushing it. These values are read back in
`bt_remotes_profile_activate` and applied when launching the relevant remote view in
`start_on_event` (e.g. `hid_media_set_mode`, `hid_tiktok_set_mode`).

**Persistence is table-driven**: every numeric per-remote setting is one row in
`bt_remotes_profile_settings[]` (`bt_remotes.c`) — `{key, offsetof(Hid, field), width,
validation, min, max, default, step}` — which drives both the save loop in
`save_profile_menu_cfg` and the read/validate loop in `profile_activate`. To add a setting:
add the `Hid` field, one table row, and the settings-scene UI row.

---

## Adding a New Scene

1. Add a `.c` file in `scenes/` following the `on_enter` / `on_event` / `on_exit` pattern. (`*.c`
   is globbed by `application.fam`, so no build-file edit is needed.)
2. Add `ADD_SCENE(bt_remotes, my_scene, MyScene)` to `scenes/bt_remotes_scenes.h`. The enum value
   `BtRemotesSceneMyScene` and the handler-table entries are generated from this macro.
3. Wire navigation from an existing scene using `scene_manager_next_scene` or
   `scene_manager_search_and_switch_to_previous_scene`.
4. If the scene needs a new view, add the view ID to `views.h`, add a lazy-alloc case for it in
   `bt_remotes_view_ensure` (called before first navigation), and a guarded teardown in
   `bt_remotes_free`. Remote views and the FileBrowser are built on first use, not at startup;
   only the Start menu (`hid_remote_menu`) and the shared GUI modules (`submenu`,
   `var_item_list`, `dialog`, `text_input`, `popup`, `help_widget`) are allocated eagerly in
   `bt_remotes_alloc`. Anything that touches a lazily-built view outside its own scene (e.g. the
   BT status fanout) must NULL-check the pointer.

---

## Profile Operations Reference

| Function | What it does |
|---|---|
| `bt_remotes_profile_load_list(app)` | Scans `profiles/` for `*.cfg`, fills `profile_list[]`, then applies `profile_order_str` ordering. |
| `bt_remotes_profile_create(app)` | New random MAC; writes profile `.cfg` (inherits `default_ble_name`); removes stale active keys. Callers follow up with `profile_activate` to load the identity into memory. |
| `bt_remotes_profile_activate(app)` | Loads the profile `.cfg`'s name + MAC into `app->ble_hid_cfg` and copies profile `.keys` → active keys (or clears keys if none). **Also** loads per-profile `menu_order`/`menu_hidden` + per-remote settings (generic `menu_item_count` migration; legacy V1/V2 arms for older files). Returns `false` if `.cfg` missing. |
| `bt_remotes_profile_save(app)` | Copies active `.bt_hid.keys` → profile `.keys`, then re-snapshots the full profile `.cfg` via `save_profile_menu_cfg`. Returns `false` if no active keys file exists yet. |
| `bt_remotes_profile_rename(app)` | Renames profile `.cfg`/`.keys`/`.pins` and rewrites the entry in `profile_order_str` (then `save_app_cfg`). Old name in `app->pending_name`, new in `app->active_profile`. |
| `bt_remotes_profile_reset(app)` | New random MAC (name preserved) into `ble_hid_cfg`; persists via `save_profile_menu_cfg`; then `profile_clear_pairing` (deletes profile + active key files). |
| `bt_remotes_profile_delete(app)` | Removes profile `.cfg`, `.keys` and `.pins` (tolerates already-absent). |
| `bt_remotes_profile_clear_pairing(app)` | Deletes both key files (active + profile) without touching the BLE stack — safe whether BLE is running or stopped. |
| `bt_remotes_save_profile_menu_cfg(app)` | Writes the full profile `.cfg` (name+mac+menu+per-remote settings). No-op if no active profile. |
| `bt_remotes_load_app_cfg(app)` | Reads `app.cfg` → `default_ble_name`, `vibro_mode`, `back_hold_exit_s`, `profile_order_str`. Sets defaults if file/keys missing. |
| `bt_remotes_save_app_cfg(app)` | Writes `default_name`, `vibro_mode`, `back_hold_exit_s`, `profile_order` to `app.cfg`. |

(Collection/pins ops mirror these — see `bt_remotes_collection_*`, `bt_remotes_pinned_load/save`,
`bt_remotes_pin_add/remove`. Gestures are read-only: just `bt_remotes_gesture_load_list` and
`bt_remotes_gesture_path`.)

**Static-random BLE MAC format**: `mac[5] |= 0xC0` sets bits 47:46 to `11`, which is required by
the BLE spec for static random addresses. Always do this when generating a new MAC.

---

## Common Gotchas

### Clearing pairing must not touch the BLE stack while it is stopped
`furi_hal_bt_start_advertising()` is undefined behaviour without an active BLE profile, so pairing
is cleared purely on disk via `bt_remotes_profile_clear_pairing` (deletes the key files). It is
safe regardless of BLE state — the `unpair` scene `stop_ble`s first, then clears. (An earlier
`bt_hid_remove_pairing` that cycled advertising was removed for this reason.)

### `bt_remotes_profile_activate` reloads `ble_hid_cfg` from the profile `.cfg`
`profile_activate` overwrites the in-memory `app->ble_hid_cfg` (and all per-profile settings) with
whatever the profile `.cfg` on disk says. If you change a per-profile value in memory without
persisting it via `bt_remotes_save_profile_menu_cfg`, the next `profile_activate` (e.g. the local
cycle in `scene_rename` after a Bluetooth Name edit, or in `scene_reset_bt_name` / `unpair`)
silently reverts it.
**Persist to the profile's `.cfg` before any path that may re-activate** — settings scenes do this
in `on_exit`; rename/reset do it inline before cycling BLE.

### `on_exit` fires for both forward and backward navigation
You cannot reliably use `on_exit` to detect whether the user pressed Back vs. selected a menu item.
Use `on_event` with `SceneManagerEventTypeBack` instead.

### `profile_select` auto-advance guard
`profile_select_on_enter` checks `app->ble_started` and, if true, immediately sends
`BtRemotesProfileSelectEventAutoAdvance` to skip directly to the Start scene. This handles `profile_new`
creating a profile and popping back, and a `.btremote` launch of an immediate-connect profile (the
entry point starts BLE before pushing ProfileSelect). If you stop BLE and clear `active_profile`
before navigating to ProfileSelect (as the Start Back handler does), this guard will be false and
the full profile list will render.

### The rename scene is context-aware
`bt_remotes_scene_rename` behaves differently depending on `active_profile`:
- **No profile** → edits `app->default_ble_name`, saves via `bt_remotes_save_app_cfg`. No BLE.
- **Profile active** → edits `app->ble_hid_cfg.name` (the text-input writes into it directly), then
  `bt_remotes_save_profile_menu_cfg` (full profile `.cfg`), then cycles BLE locally:
  `stop_ble` (if started) → `profile_activate` → `start_ble_if_immediate`, so the new name
  advertises right away.
