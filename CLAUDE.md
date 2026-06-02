# CLAUDE.md — bt_remotes

Working notes for AI agents on this app. Keep this short; put detail in the docs below.

- **Users / what it does:** [`README.md`](README.md)
- **Architecture & internals (the technical source of truth):** [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)

`bt_remotes` is a BLE HID remote-control FAP for the Flipper Zero. It has its own independent git
remote (`https://github.com/Skordio/flipper-bt-remotes.git`) even though it lives inside the
Momentum firmware tree at `applications_user/bt_remotes/`.

---

## Rules that bite if you forget them

1. **Momentum firmware only — never remove or bypass the startup guard.**
   `bt_remotes_app` calls `bt_remotes_firmware_supported()` (origin == `"Momentum"`) *before* any
   alloc/BLE work and bails with a dialog otherwise. The app pokes a Momentum-only `GapConfig`
   field; on other firmware it corrupts memory and hard-faults the device. Do **not** move
   alloc/BLE above the guard. (Full reasoning in ARCHITECTURE → *Firmware Requirement*.)

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

6. **BLE state:** `start_ble` **asserts** `!ble_started` (stop first; it's not a guard). Only the
   Settings flow stops BLE — Ducky Scripts / Custom Gestures / pinned launches do not. Use
   `bt_remotes_profile_clear_pairing` (not `bt_hid_remove_pairing`) whenever BLE is stopped.

7. **Two separate script engines:** Ducky Scripts use `helpers/ducky_runner.c`; Custom Gestures
   use `helpers/gesture_runner.c` (its own lowercase-verb language, with `run <name>` inheritance).
   They share only the worker-thread/stop/popup *pattern*, not code — don't merge them.

---

## Orientation (where to look)

- **`bt_remotes.h`** — `Hid` struct, all constants, `BtRemotesStartIndex` enum, decls.
- **`bt_remotes.c`** — firmware guard, BLE lifecycle, profile/collection/gesture/pins I/O, alloc/entry.
- **`scenes/bt_remotes_scenes.h`** — authoritative scene list (`ADD_SCENE` macro).
- **`scenes/bt_remotes_scene_start.c`** — Start menu build + routing + `bt_remotes_menu_default[]`.
- **`views/hid_remote_menu.c`** — the reorderable/pinnable Start-menu view.
- **`helpers/`** — `ble_hid_ext_profile` (custom MAC/name), `ducky_runner`, `gesture_runner`.

When you change persistence formats, menu indices, or the BLE lifecycle, update
`docs/ARCHITECTURE.md` to match.
