# bt_remotes — Architecture & Internals

Developer reference for the `bt_remotes` Flipper Zero app. For a user-facing overview see
[`../README.md`](../README.md); for the short list of rules/invariants an agent must follow see
[`../CLAUDE.md`](../CLAUDE.md). This document is the detailed technical source of truth.

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

Any one missing import makes the loader refuse the FAP. The **16 built-in icons are the universal
blocker** across all three other firmwares.

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
│   ├── bt_remotes_scene_settings.c   bt_remotes_scene_help.c
│   ├── bt_remotes_scene_hide_items.c   bt_remotes_scene_reset_menu.c
│   │   # Per-remote settings:
│   ├── bt_remotes_scene_remote_type_settings.c   bt_remotes_scene_remote_settings_help.c
│   ├── bt_remotes_scene_keynote_settings.c   bt_remotes_scene_media_settings.c
│   ├── bt_remotes_scene_tiktok_settings.c
│   │   # Ducky Scripts + Collections:
│   ├── bt_remotes_scene_custom_actions.c   bt_remotes_scene_custom_actions_run.c
│   ├── bt_remotes_scene_collection_list.c   bt_remotes_scene_collection_view.c
│   ├── bt_remotes_scene_collection_create.c   bt_remotes_scene_collection_edit.c
│   ├── bt_remotes_scene_collection_delete.c   bt_remotes_scene_collection_pin.c
│   │   # Custom Gestures:
│   ├── bt_remotes_scene_gesture_list.c   bt_remotes_scene_gesture_create.c
│   ├── bt_remotes_scene_gesture_edit.c   bt_remotes_scene_gesture_run.c
│   └── bt_remotes_scene_gesture_pin.c
└── views/
    ├── hid_remote_menu.c/h    # Reorderable/hideable/pinnable Start-menu view (BtRemotesMenuEntry)
    ├── hid_keyboard.c/h   hid_keynote.c/h   hid_numpad.c/h
    ├── hid_media.c/h   hid_music_macos.c/h   hid_movie.c/h   hid_tiktok.c/h
    ├── hid_mouse.c/h   hid_mouse_clicker.c/h
    ├── hid_mouse_jiggler.c/h   hid_mouse_jiggler_stealth.c/h
    └── hid_ptt.c/h   hid_ptt_menu.c/h
```

(Plus `assets/` — PNG icons for the remote views — and `bt_remotes_10px.png`, the app icon.)

---

## Key Constants (`bt_remotes.h`)

| Constant | Value | Purpose |
|---|---|---|
| `BT_REMOTES_PROFILES_DIR` | `APP_DATA_PATH("profiles")` | Profile + `.pins` storage directory |
| `BT_REMOTES_CFG_PATH` | `APP_DATA_PATH(".bt_hid.cfg")` | Active profile cfg (name + MAC + menu layout) |
| `BT_REMOTES_APP_CFG_PATH` | `APP_DATA_PATH("app.cfg")` | App-level config (default name, vibro, profile order) |
| `BT_REMOTES_CFG_EXT` / `_KEYS_EXT` | `.cfg` / `.keys` | Per-profile config / bonding-keys extensions |
| `BT_REMOTES_PROFILE_NAME_LEN` | `32` | Max profile name length (including NUL) |
| `BT_REMOTES_PROFILE_MAX_COUNT` | `16` | Max number of profiles |
| `BT_REMOTES_MENU_ITEM_COUNT` | `16` | Number of fixed Start-menu items (enum `BtRemotesStartIndex*`) |
| `BT_REMOTES_PINNED_MAX` | `16` | Max pinned collections/gestures on the Start menu |
| `BT_REMOTES_MENU_ORDER_LEN` | `32` | `MENU_ITEM_COUNT + PINNED_MAX` — length of `menu_order[]` |
| `BT_REMOTES_MENU_ITEM_COUNT_V1` / `_ORDER_LEN_V1` | `15` / `31` | Pre-Custom-Gestures layout, for migrating saved `menu_order` |
| `BT_REMOTES_COLLECTION_DIR` | `APP_DATA_PATH("collections")` | Ducky Script Collections directory |
| `BT_REMOTES_COLLECTION_EXT` | `.collection` | Collection file extension |
| `BT_REMOTES_COLLECTION_MAX` / `_SCRIPT_MAX` | `16` / `32` | Max collections / scripts-per-collection |
| `BT_REMOTES_GESTURE_DIR` | `APP_DATA_PATH("gestures")` | Custom Gestures library directory (global) |
| `BT_REMOTES_GESTURE_EXT` | `.gesture` | Gesture file extension |
| `BT_REMOTES_GESTURE_MAX` / `_NAME_LEN` | `32` / `32` | Max gestures / gesture-name length |
| `GESTURE_LINE_MAX` / `GESTURE_LINE_LEN` | `64` / `64` | Max lines per gesture / chars per line (in `gesture_runner.h`) |
| `HID_BT_KEYS_STORAGE_NAME` | `.bt_hid.keys` | Active bonding keys filename |
| `FURI_HAL_BT_ADV_NAME_LENGTH` | `20` | Max BLE device name length (from furi_hal_version.h) |

---

## The `Hid` Struct (abridged — see `bt_remotes.h` for the full definition)

The struct is large; these are the fields agents touch most. Names/types are exact.

```c
struct Hid {
    // BLE
    FuriHalBleProfileBase* ble_hid_profile;   // live profile handle (NULL when stopped)
    BleProfileHidExtParams ble_hid_cfg;       // .name[20], .mac[6] — loaded from .bt_hid.cfg
    bool ble_started;                          // true while HID profile is running
    Bt* bt; Gui* gui; NotificationApp* notifications; Storage* storage;

    // GUI infra + view modules
    ViewDispatcher* view_dispatcher; SceneManager* scene_manager;
    Submenu* submenu; VariableItemList* var_item_list; DialogEx* dialog;
    TextInput* text_input; Popup* popup; Widget* help_widget;
    HidRemoteMenu* hid_remote_menu;            // the Start menu
    HidKeynote* hid_keynote; HidKeyboard* hid_keyboard; /* ...all remote views... */

    // Collections + pins (Ducky Scripts)
    char    collection_names[16][32];   uint8_t collection_count;
    char    pinned_collections[16][32]; uint8_t pinned_kinds[16];  // 0=collection, 1=gesture
    uint8_t pinned_count;
    char    editing_collection_name[32];
    char    editing_collection_scripts[32][256]; uint8_t editing_collection_script_count;

    // Custom Gestures (global library + editing buffer)
    char    gesture_names[32][32]; uint8_t gesture_count;
    char    editing_gesture_name[32];
    char    editing_gesture_lines[GESTURE_LINE_MAX][GESTURE_LINE_LEN];
    uint8_t editing_gesture_line_count;

    // Profile management
    char active_profile[32];   // filename stem of the active profile; "" = none selected
    char pending_name[32];     // old profile name held during a rename operation
    char default_ble_name[20]; // default BT name applied when creating new profiles
    char profile_list[16][32]; uint8_t profile_count;

    // Per-profile remote-type settings
    uint8_t keynote_back_key, media_mode, media_mouse_switch, tiktok_scroll_mode;
    uint16_t tiktok_gesture_inset, tiktok_gesture_margin, tiktok_gesture_swipe;

    // App-level + menu layout
    uint8_t  vibro_mode;                          // 0=Neither 1=Disconnect 2=Connect 3=Both
    uint8_t  menu_order[BT_REMOTES_MENU_ORDER_LEN]; // 0xFF = unused slot; >=16 = pinned slot
    uint32_t menu_hidden;                          // bitmask: bit i set → item i hidden
    char     profile_order_str[...];               // pipe-separated saved profile order

    // Ducky Scripts file browser + runners
    FileBrowser* file_browser; FuriString* file_browser_result;
    DuckyRunner* ducky_runner; GestureRunner* gesture_runner;
    char pending_script_path[256];

    // Post-pairing auto-save timer
    FuriTimer* pair_save_timer; uint8_t pair_save_attempts;
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
  menu_order: <32 uint32 values>     # fixed idx 0‥15, pinned slot 16‥31, 0xFF = unused
  menu_hidden: <uint32 bitmask>      # bit i set → fixed item i hidden
  keynote_back_key: <uint32>         # KeynoteBackKey enum
  media_mode: <uint32>               # MediaMode enum
  media_mouse_switch: <uint32>       # 0/1
  tiktok_scroll_mode: <uint32>       # TikTokScrollMode enum
  tiktok_gesture_inset / _margin / _swipe: <uint32>   # px tunables
  ```
  `bt_remotes_save_profile_menu_cfg` writes the **full** set above. Note `bt_hid_save_cfg` (used
  by rename/reset) writes only `name`+`mac` to the **active** `.bt_hid.cfg` mirror — the full
  per-profile cfg is then re-snapshotted via `save_profile_menu_cfg`. Old cfg files missing any
  key load with safe defaults and are migrated on read (see the V1 migration arm in
  `bt_remotes_profile_activate`).
- **`{name}.keys`** — BLE bonding keys binary blob (written by the BT service).
- **`{name}.pins`** — `Flipper BT Collection Pins` v1. Per-profile list of Start-menu pins:
  ```
  Filetype: Flipper BT Collection Pins
  Version: 1
  count: 2
  collection_0: My Macros   kind_0: 0    # 0 = collection
  collection_1: Head Track  kind_1: 1    # 1 = gesture
  ```
  Older `.pins` files omit `kind_N` and default to collection (kind 0).

The **active** profile's cfg/keys are mirrored to `APP_DATA_PATH(".bt_hid.cfg")` /
`APP_DATA_PATH(".bt_hid.keys")` — these are what the BT stack reads at runtime.

### App-level config — `APP_DATA_PATH("app.cfg")`

`Flipper BT Remotes App Config` v1 — settings shared across all profiles:
```
Filetype: Flipper BT Remotes App Config
Version: 1
default_name: Flipper Zero       # BT name new profiles inherit ("" if unset)
vibro_mode: 1                    # 0=Neither 1=Disconnect 2=Connect 3=Both (default 1)
profile_order: Keyboard|Mouse|…  # pipe-separated saved profile display order
```
Missing keys fall back to defaults (default_name "", vibro 1, no profile order). Legacy files with
the old `disconnect_vibro` bool simply fail the uint32 read and default to 1.

### Collections (Ducky Scripts) — `APP_DATA_PATH("collections/")`

- **`{name}.collection`** — `Flipper BT Ducky Collection` v1: `count` + `script_0`, `script_1`, …
  (each a full path to a `.txt` DuckyScript). Up to `BT_REMOTES_COLLECTION_SCRIPT_MAX` (32).

### Custom Gestures (global) — `APP_DATA_PATH("gestures/")`

- **`{name}.gesture`** — `Flipper BT Custom Gesture` v1: `count` + `line_0`, `line_1`, … (one
  gesture command per key). Global = shared across all profiles. See **Custom Gestures**.

---

## BLE Lifecycle

### `bt_remotes_start_ble(Hid* app)`

1. `furi_assert(!app->ble_started)` — **must not** be called while already started
2. Resets the blue LED off
3. Sets the BT keys storage path to `APP_DATA_PATH(".bt_hid.keys")`
4. Registers `bt_remotes_connection_status_changed_callback`
5. Loads cfg from `.bt_hid.cfg` into `app->ble_hid_cfg` (`bt_remotes_load_cfg`)
6. Starts the HID profile with `bt_profile_start(app->bt, ble_profile_hid_ext, &app->ble_hid_cfg)`
   and `furi_check`s the handle
7. Starts advertising; sets `app->ble_started = true`

**Registers the status callback every time it is called** — the callback would otherwise be lost
after any stop/start cycle. Note it **asserts** `!ble_started` (it does not guard); callers must
stop BLE first. `ble_profile_hid_ext` is the custom Momentum-only profile template that injects
the per-profile MAC/name (see **Firmware Compatibility**).

### `bt_remotes_stop_ble(Hid* app)`

Has an `if(!app->ble_started) return;` guard — safe to call redundantly. Then:

1. Stops the pairing-save timer (`furi_timer_stop(app->pair_save_timer)`)
2. Clears the status callback (`bt_set_status_changed_callback(NULL, NULL)`)
3. Resets the blue LED off
4. Disconnects (`bt_disconnect`) and waits 200 ms
5. Resets keys path to default (`bt_keys_storage_set_default_path`)
6. Restores default BT profile (`furi_check(bt_profile_restore_default)`)
7. Sets `app->ble_started = false`

### Connection Status Callback

```c
static void bt_remotes_connection_status_changed_callback(BtStatus status, void* context)
```

Fires on every BT status change. It:
- Sets the blue LED on connect / off otherwise
- Fires a vibro pulse per `vibro_mode` (connect if mode 2/3, disconnect if mode 1/3)
- Updates connected state in every remote-control view

**Pairing auto-save (the important part).** When a profile is active and the status becomes
connected, the keys file may or may not exist yet:
- **Reconnect** (`profiles/{name}.keys` already on disk) → call `bt_remotes_profile_save(app)`
  immediately to capture any refreshed key material.
- **First-time pairing** (no saved keys yet) → the BLE link connects *before* SMP bonding
  finishes, so `.bt_hid.keys` doesn't exist at callback time. Instead of saving now, it starts
  `pair_save_timer` (period `PAIR_SAVE_POLL_MS` = 200 ms) which polls for the keys file and saves
  once the stack writes it (up to `PAIR_SAVE_MAX_ATTEMPTS` = 25 ≈ 5 s).

On **disconnect** it stops the pairing-save timer. (This polling approach replaced an earlier
assumption that the BT service always writes keys before firing the status callback.)

---

## Start Menu (`hid_remote_menu` + `menu_order` / `menu_hidden` / pins)

The Start screen is a custom reorderable list, not a submenu. Item identity is the
`BtRemotesStartIndex` enum (0‥15, Settings last at 15). `bt_remotes_menu_default[]` (in
`bt_remotes_scene_start.c`) maps each index to a label and is kept in **enum order** so
`bt_remotes_menu_default[i].index == i` — `hide_items` and several guards rely on this.

- **`menu_order[32]`** (per profile): visual order. Values `0‥15` = fixed items, `16‥31` = pinned
  slot `value-16` into `pinned_collections[]`, `0xFF` = unused slot.
- **`menu_hidden`** (per profile): bitmask, bit `i` set → fixed item `i` hidden.
- **Pins**: `pinned_collections[i]` + `pinned_kinds[i]` (0 = collection, 1 = gesture), persisted
  per profile in `{name}.pins`. A pinned slot launches `CollectionView` or `GestureRun` by kind.

`start_on_enter` rebuilds the displayed list from `menu_order`, then **appends** (a) any fixed
item missing from a saved order — so items added in a firmware update (e.g. Custom Gestures)
appear on existing profiles without a "Reset Menu Order", and (b) any newly-pinned collection not
yet in `menu_order`. **Settings must stay the highest-indexed fixed item** — `hide_items` and the
load-time "never hide Settings" guard depend on it. When inserting a new fixed item, add it
*before* Settings and bump the V1 migration (`BT_REMOTES_MENU_ITEM_COUNT_V1` / `_ORDER_LEN_V1`).

---

## Scene Navigation Map

```
ProfileSelect ──► ProfileNew ──► (back to ProfileSelect, auto-advance to Start)
    │
    ├──► Start (hid_remote_menu: reorderable/hideable items + pinned slots) ◄──────┐
    │      │  [Back] stops BLE, clears active_profile, returns to ProfileSelect    │
    │      │                                                                        │
    │      ├──► Main (remote views: Keynote[/Vertical], Keyboard, Numpad, Media,   │
    │      │      Apple Music macOS, Movie, TikTok, Mouse[/Clicker/Jiggler/        │
    │      │      Stealth], PushToTalk→PTT menu). [Back from Main returns to Start] │
    │      │                                                                        │
    │      ├──► CustomActions ("Ducky Scripts") ─► Browse Files (file_browser)     │
    │      │         │                                 └─► CustomActionsRun         │
    │      │         └─► CollectionList ─► CollectionView/Create/Edit/Delete/Pin    │
    │      │                                                                        │
    │      ├──► GestureList ("Custom Gestures") ─► GestureCreate/Edit/Run/Pin      │
    │      │                                                                        │
    │      ├──► (pinned slot) ─► CollectionView (kind 0) or GestureRun (kind 1)    │
    │      │                                                                        │
    │      └──► Settings ──► Rename (BT name, per-profile or default)             │
    │                   ├──► ProfileRenameFile, ResetProfile, Unpair, SaveProfile   │
    │                   ├──► HideItems, ResetMenu (Start-menu visibility/order)     │
    │                   ├──► RemoteTypeSettings ─► Keynote/Media/TikTokSettings     │
    │                   │                          └─► RemoteSettingsHelp           │
    │                   ├──► Help                                                   │
    │                   └──► DeleteProfile ──────────────────────────────────────►┘
    │                         (deletes profile files, clears active_profile → ProfileSelect)
    │
    └──► Settings (same scene, entered without a profile; only shows app-level items)
```

`bt_remotes_scenes.h` is the authoritative scene list. Start does **not** use a plain submenu — it
uses the custom `hid_remote_menu` view (see **Start Menu**). Routing of the selected index lives
in `bt_remotes_scene_start_on_event`.

**Critical navigation rule**: `bt_remotes_stop_ble` is called in `start_on_event` when navigating
to Settings. `bt_remotes_start_ble` is called in `settings_on_event` when handling
`SceneManagerEventTypeBack` — but only if `!app->ble_started && active_profile[0] != '\0'`. The
`settings_on_event` Back handler returns `false` so the scene manager still pops the scene. (Ducky
Scripts / Custom Gestures / pinned-slot launches do **not** stop BLE — only Settings does.)

---

## Ducky Scripts & Collections

Reached from the Start menu item **"Ducky Scripts"** (`BtRemotesStartIndexCustomActions` →
`bt_remotes_scene_custom_actions`). That scene is a 2-item hub:

- **Browse Files** → `file_browser` over `.txt` DuckyScript files; selecting one stores its path
  in `pending_script_path` and pushes `CustomActionsRun`, which executes it via **`ducky_runner`**
  (`helpers/ducky_runner.c` — a DuckyScript parser + worker-thread runner against the BLE HID
  profile; interruptible, with a done/error popup).
- **Collections** → `CollectionList`: named groups of script paths stored in
  `collections/{name}.collection`. Scenes: `collection_create` (name + validate), `collection_edit`
  (add/remove scripts), `collection_view` (run the collection's scripts in sequence),
  `collection_delete`, `collection_pin` (pin to the Start menu, kind 0).

`ducky_runner` and the gesture runner share only the **threading/stop/popup pattern**, not code.

---

## Custom Gestures

A brand-new minimal mouse/keyboard scripting language with **its own** parser + runner
(`helpers/gesture_runner.c/.h`) — deliberately *not* built on `ducky_runner`. Reached from the
Start menu item **"Custom Gestures"** (`BtRemotesStartIndexCustomGestures` → `gesture_list`). The
library is **global** (`gestures/*.gesture`, shared across profiles); gestures can be pinned to the
Start menu (kind 1) and run from a pin or from the list.

**Language** (one command per line; `#`/blank lines ignored; verbs case-insensitive):

| Command | Meaning |
|---|---|
| `anchor tl\|tr\|bl\|br` | Slam the cursor to a screen corner (repeatable origin) |
| `move <dx> <dy>` | Relative mouse move (px), auto-chunked into int8 steps |
| `tap` | Finger tap = left press + release |
| `click left\|right` | Mouse button click |
| `drag <dx> <dy>` | Left press → stepped move → release |
| `scroll <n>` | Mouse wheel (signed) |
| `wait <ms>` | Delay (interruptible) |
| `key <combo>` | Keyboard key/combo, e.g. `key enter`, `key cmd space` |
| `type <text>` | Type a literal string |
| `run <name>` | **Script inheritance** — run another gesture by name |

**Authoring** is on-device, line-by-line: `gesture_create` (name) → `gesture_edit` (add/edit/delete
lines + a Help page). Each line is validated by `gesture_line_validate` (unknown verbs/keys
rejected). `gesture_run` executes via `gesture_runner` with a Running/Done/Error popup; Back stops
the run.

**Inheritance** (`run <name>`): `gesture_runner` executes recursively. `gesture_run_file` resolves
`<name>` as a **sibling** in the same `gestures/` dir, capped at `GESTURE_MAX_DEPTH` (5) to prevent
cycles; a missing child in a nested `run` is skipped (only a missing **top-level** file is an
error). The worker thread uses a 4 KB stack to accommodate the recursion. File format is `count` +
`line_0…line_N` (see **On-Disk File Formats**).

---

## Per-Remote Settings (per profile)

Reached from **Settings → Remote Settings** (`remote_type_settings` hub). Three sub-scenes, each a
`VariableItemList`, all persisted in the profile `.cfg`:

- `keynote_settings` — `keynote_back_key` (`KeynoteBackKey`: Delete/Left/Escape/None).
- `media_settings` — `media_mode` (`MediaMode`: Legacy/Improved) + `media_mouse_switch` (short Back
  opens the mouse sub-view).
- `tiktok_settings` — `tiktok_scroll_mode` (Wheel/Gesture) + three px tunables
  `tiktok_gesture_inset` / `_margin` / `_swipe` (ranges/defaults defined in `bt_remotes.h`).

`remote_settings_help` is a shared help scene; the launching settings scene stores the
`RemoteSettingsHelpTopic` as its scene state before pushing it. These values are read back in
`bt_remotes_profile_activate` and applied when launching the relevant remote view in
`start_on_event` (e.g. `hid_media_set_mode`, `hid_tiktok_set_mode`).

---

## Adding a New Scene

1. Add a `.c` file in `scenes/` following the `on_enter` / `on_event` / `on_exit` pattern. (`*.c`
   is globbed by `application.fam`, so no build-file edit is needed.)
2. Add `ADD_SCENE(bt_remotes, my_scene, MyScene)` to `scenes/bt_remotes_scenes.h`. The enum value
   `BtRemotesSceneMyScene` and the handler-table entries are generated from this macro.
3. Wire navigation from an existing scene using `scene_manager_next_scene` or
   `scene_manager_search_and_switch_to_previous_scene`.
4. If the scene needs a new view, add the view ID to `views.h`, allocate the view in
   `bt_remotes_alloc`, and free it in `bt_remotes_free`. Many scenes instead reuse the shared
   modules already allocated (`submenu`, `var_item_list`, `dialog`, `text_input`, `popup`,
   `help_widget`, `file_browser`).

---

## Profile Operations Reference

| Function | What it does |
|---|---|
| `bt_remotes_profile_load_list(app)` | Scans `profiles/` for `*.cfg`, fills `profile_list[]`, then applies `profile_order_str` ordering. |
| `bt_remotes_profile_create(app)` | New random MAC; writes profile `.cfg` (inherits `default_ble_name`) + active `.bt_hid.cfg`; removes stale active keys. |
| `bt_remotes_profile_activate(app)` | Copies profile `.cfg` → `.bt_hid.cfg` and profile `.keys` → active keys (or clears keys if none). **Also** loads per-profile `menu_order`/`menu_hidden` + per-remote settings (with V1 migration). Returns `false` if `.cfg` missing. |
| `bt_remotes_profile_save(app)` | Copies active `.bt_hid.keys` → profile `.keys`, then re-snapshots the full profile `.cfg` via `save_profile_menu_cfg`. Returns `false` if no active keys file exists yet. |
| `bt_remotes_profile_rename(app)` | Renames profile `.cfg`/`.keys`. Old name in `app->pending_name`, new in `app->active_profile`. |
| `bt_remotes_profile_reset(app)` | New random MAC (name preserved); saves via `bt_hid_save_cfg` + `save_profile_menu_cfg`; deletes profile + active key files. |
| `bt_remotes_profile_delete(app)` | Removes profile `.cfg` and `.keys` (tolerates already-absent). |
| `bt_remotes_profile_clear_pairing(app)` | Deletes both key files (active + profile) without touching BLE stack. Use instead of `bt_hid_remove_pairing` whenever BLE is stopped. |
| `bt_remotes_save_profile_menu_cfg(app)` | Writes the full profile `.cfg` (name+mac+menu+per-remote settings). No-op if no active profile. |
| `bt_remotes_load_app_cfg(app)` | Reads `app.cfg` → `default_ble_name`, `vibro_mode`, `profile_order_str`. Sets defaults if file/keys missing. |
| `bt_remotes_save_app_cfg(app)` | Writes `default_name`, `vibro_mode`, `profile_order` to `app.cfg`. |

(Collection/gesture/pins ops mirror these — see `bt_remotes_collection_*`, `bt_remotes_gesture_*`,
`bt_remotes_pinned_load/save`.)

**Static-random BLE MAC format**: `mac[5] |= 0xC0` sets bits 47:46 to `11`, which is required by
the BLE spec for static random addresses. Always do this when generating a new MAC.

---

## Common Gotchas

### `bt_hid_remove_pairing` requires BLE to be active
This function calls `furi_hal_bt_start_advertising()`, which is undefined behaviour without an
active BLE profile. **Do not call it from the Settings scene or any scene entered from Settings** —
BLE is always stopped before entering Settings. Use `bt_remotes_profile_clear_pairing` instead.

### `bt_remotes_profile_activate` overwrites `.bt_hid.cfg`
When navigating Back from Settings, `settings_on_event` calls `bt_remotes_profile_activate` before
`bt_remotes_start_ble`. `profile_activate` copies the profile's `.cfg` → `.bt_hid.cfg`. If you
saved a change only to `.bt_hid.cfg` (e.g. via `bt_hid_save_cfg`, which writes only name+mac) but
not to the profile's `.cfg`, it will be silently overwritten. **Always persist changes to the
profile's `.cfg` immediately** — the rename scene calls `bt_remotes_save_profile_menu_cfg(app)`
after `bt_hid_save_cfg`, which rewrites the full profile cfg (name + mac + menu layout + per-remote
settings).

### `on_exit` fires for both forward and backward navigation
You cannot reliably use `on_exit` to detect whether the user pressed Back vs. selected a menu item.
Use `on_event` with `SceneManagerEventTypeBack` instead.

### `profile_select` auto-advance guard
`profile_select_on_enter` checks `app->ble_started` and, if true, immediately sends
`BtRemotesProfileSelectEventAutoAdvance` to skip directly to the Start scene. This handles the case
where `profile_new` creates a profile and pops back. If you stop BLE and clear `active_profile`
before navigating to ProfileSelect (as the Start Back handler does), this guard will be false and
the full profile list will render.

### The rename scene is context-aware
`bt_remotes_scene_rename` behaves differently depending on `active_profile`:
- **No profile** → edits `app->default_ble_name`, saves via `bt_remotes_save_app_cfg`. No BLE.
- **Profile active** → edits `app->ble_hid_cfg.name`, then `bt_hid_save_cfg` (active `.bt_hid.cfg`)
  + `bt_remotes_save_profile_menu_cfg` (full profile `.cfg`). BLE is already stopped; restart
  happens on Back from Settings (which re-activates the profile and loads the new name).
