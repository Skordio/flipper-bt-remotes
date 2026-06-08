# CLAUDE.md — bt_remotes

Working notes for AI agents on this app. Keep this short; put detail in the docs below.

- **Users / what it does:** [`README.md`](README.md)
- **Architecture & internals (the technical source of truth):** [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)

`bt_remotes` is a BLE HID remote-control FAP for the Flipper Zero. It has its own independent git
remote (`https://github.com/Skordio/flipper-bt-remotes.git`) even though it lives inside the
Momentum firmware tree at `applications_user/bt_remotes/`.

---

## Rules that bite if you forget them

1. **Momentum-targeted, but for a mundane reason — and there's no runtime guard.** The app simply
   won't load on stock/Unleashed/RogueMaster: it imports app-API symbols only Momentum exports —
   16 built-in icons (which the app already bundles in `assets/` but references via firmware
   `&I_…`), plus `strtok` and `variable_item_list_set_header`. Other firmwares reject it with
   "Update Firmware to use with this Application" (`MissingImports`). It is **not** a BLE/custom-MAC
   issue — `GapConfig` is identical everywhere. (An earlier `version_get_firmware_origin` startup
   guard, based on a wrong BT-address theory, was removed.) To re-check after symbol changes:
   `arm-none-eabi-nm -u <built>.fap` vs a firmware's `api_symbols.csv` `+` entries. Full analysis:
   ARCHITECTURE → *Firmware Compatibility*.

2. **Git: this app has its OWN remote.** Commit/push with `git -C` targeting the `bt_remotes/`
   directory, **not** the firmware root. Any temp commit-message file must live **outside** the
   repo. **Commit/push only when the user explicitly asks.** No force-push without explicit
   authorization.

3. **Build/deploy (Windows, PowerShell):**
   ```
   .\fbt.cmd build  APPSRC=applications_user/bt_remotes
   .\fbt.cmd launch APPSRC=applications_user/bt_remotes      # build + deploy + run
   ```
   Redirect noisy output (`*> "$env:TEMP\log"`) and check `$LASTEXITCODE`. The Flipper's COM port
   is flaky: it can re-enumerate (COM5→COM3) and briefly report busy/access-denied — poll until
   openable and retry. For pushing files with `scripts/storage.py`, pass `-p COMx` explicitly and
   run it from **PowerShell** (git-bash/MSYS mangles `/ext/...` paths into Windows paths). Toolchain
   python: `toolchain/current/python/python.exe`.

4. **Settings must stay the last fixed Start-menu item** (`BtRemotesStartIndexSettings`, highest
   index). `hide_items` and a load-time guard depend on it. To add a fixed menu item, insert it
   *before* Settings and bump the V1 migration constants (`BT_REMOTES_MENU_ITEM_COUNT_V1` /
   `_ORDER_LEN_V1`). See ARCHITECTURE → *Start Menu*.

5. **Persisting profile changes:** per-profile settings (name, menu layout, per-remote settings)
   live in the profile `.cfg`. `bt_hid_save_cfg` writes only name+mac to the active mirror —
   always also call `bt_remotes_save_profile_menu_cfg(app)` so a change survives
   `profile_activate` (which overwrites `.bt_hid.cfg` from the profile `.cfg` on Back-from-Settings).

6. **BLE state:** `start_ble` **asserts** `!ble_started` (stop first; it's not a guard). In the
   default mode only the Settings flow stops BLE — Ducky Scripts / Custom Gestures / pinned launches
   do not. Use `bt_remotes_profile_clear_pairing` (not `bt_hid_remove_pairing`) whenever BLE is
   stopped. **Per-profile `delay_connect`:** when set, the Start scene owns BLE — `on_event` starts
   it on the way into any non-Settings destination, `on_enter` stops it on every return to the menu;
   the Settings-Back / `unpair` / `reset_profile` auto-restarts go through
   `bt_remotes_start_ble_if_immediate` (gated by `!delay_connect`). Defaults off; the `profile_new`
   flow calls `bt_remotes_profile_activate` after `profile_create` so a new profile defaults **all**
   per-profile fields (not just this flag) instead of inheriting the previous profile's in-memory state.
   **Per-profile `ducky_connect_per_run`** (independent of `delay_connect`): Ducky/Collections stay
   disconnected while browsing (`custom_actions`/`collection_view` `on_enter` stop BLE) and the run
   scene (`custom_actions_run`) connects only for a script's execution — `start_ble` → wait on
   `app->connected` (polled via `connect_wait_timer`) → **settle `ducky_connect_settle_ms`** (host
   needs time to subscribe to HID notifications, else first keys drop) → run → `stop_ble` in
   `on_exit`. `start_on_enter` restores an immediate-mode link afterward
   (`else if(!ble_started) start_ble`). Gestures unaffected. Both settings live under Settings →
   Per-Remote Settings → **DuckyScript** (`scene_ducky_settings.c`).

7. **Two separate script engines:** Ducky Scripts use `helpers/ducky_runner.c`; Custom Gestures
   use `helpers/gesture_runner.c` (its own lowercase-verb language, with `run <name>` inheritance).
   They share only the worker-thread/stop/popup *pattern*, not code — don't merge them.

---

## Orientation (where to look)

- **`bt_remotes.h`** — `Hid` struct, all constants, `BtRemotesStartIndex` enum, decls.
- **`bt_remotes.c`** — BLE lifecycle, profile/collection/gesture/pins I/O, alloc/entry.
- **`scenes/bt_remotes_scenes.h`** — authoritative scene list (`ADD_SCENE` macro).
- **`scenes/bt_remotes_scene_start.c`** — Start menu build + routing + `bt_remotes_menu_default[]`.
- **`views/hid_remote_menu.c`** — the reorderable/pinnable Start-menu view.
- **`helpers/`** — `ble_hid_ext_profile` (custom MAC/name), `ducky_runner`, `gesture_runner`.

When you change persistence formats, menu indices, or the BLE lifecycle, update
`docs/ARCHITECTURE.md` to match.
