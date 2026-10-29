# AGENTS.md — bt_remotes

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
   `&I_…`), plus `strtok`, `variable_item_list_set_header`, and `momentum_settings`. Other
   firmwares reject it with "Update Firmware to use with this Application" (`MissingImports`). It
   is **not** a BLE/custom-MAC issue — `GapConfig` is identical everywhere. (An earlier
   `version_get_firmware_origin` startup guard, based on a wrong BT-address theory, was removed.)
   **Fork caveat:** the `momentum_settings.wrap_on_hold` field the menu views read exists only in
   Skordio's Momentum fork (firmware commit `d18c70976` appends it to `MomentumSettings`). On
   upstream Momentum the app still loads, but that read lands past the end of the struct. To
   re-check after symbol changes: `toolchain/x86_64-linux/bin/arm-none-eabi-nm -u <built>.fap` vs a
   firmware's `api_symbols.csv` `+` entries. Full analysis: ARCHITECTURE → *Firmware Compatibility*.

2. **Git: this app has its OWN remote.** Commit/push with `git -C` targeting the `bt_remotes/`
   directory, **not** the firmware root. Any temp commit-message file must live **outside** the
   repo. **Commit/push only when the user explicitly asks.** No force-push without explicit
   authorization.

3. **Build/deploy (Linux, current dev env):**
   ```
   ./fbt build  APPSRC=applications_user/bt_remotes
   ./fbt launch APPSRC=applications_user/bt_remotes      # build + deploy + run
   ```
   The Flipper appears as `/dev/ttyACM0`. **Always ask the user whether the Flipper is plugged
   in and connected before running `./fbt launch`** — unless the user explicitly granted
   deploy permission in their immediately preceding message ("go ahead and deploy", "deploy it",
   etc.). Permission is per-message, not per-session; ask again on the next deploy. `./fbt build`
   alone is fine without asking (no hardware involved).

4. **Settings must stay the last fixed Start-menu item** (`BtRemotesStartIndexSettings`, highest
   index). `hide_items` and a load-time guard depend on it. To add a fixed menu item, insert it
   *before* Settings, bump `BT_REMOTES_MENU_ITEM_COUNT` (`_ORDER_LEN` derives from it), and bump
   `REMOTE_MENU_MAX_ITEMS` in `views/hid_remote_menu.h` (a `_Static_assert` in
   `scene_start.c` fails the build if you forget) — cfg files carry
   `menu_item_count` (since 2026-08), so saved menu orders migrate generically; no new migration
   arm is needed. The legacy V1/V2/33-entry arms in `bt_remotes_profile_activate` only serve
   files saved before that key existed — don't extend them. See ARCHITECTURE → *Start Menu*.

5. **Persisting profile changes:** per-profile settings (name, menu layout, per-remote settings)
   live in the profile `.cfg` — the single on-disk source of truth (there is no active-cfg mirror).
   Change values in memory (incl. `app->ble_hid_cfg` for name/mac), then persist via
   `bt_remotes_save_profile_menu_cfg(app)` before anything can call `profile_activate`
   (e.g. Back-from-Settings), which reloads all per-profile state from the `.cfg`.
   Settings scenes persist in `on_exit` (see `scene_ducky_settings.c`), not per value tick.

6. **BLE state:** `start_ble` **asserts** `!ble_started` (stop first; it's not a guard). In the
   default (immediate-connect) mode BLE stays up through the whole Settings sub-tree so the host
   doesn't drop while the user is tweaking knobs; the only Settings actions that cycle BLE are
   renaming the active profile's BT name (`scene_rename.c`) and Reset Name to Default
   (`scene_reset_bt_name.c`), so the new name advertises immediately, and Unpair
   (`scene_unpair.c`), which restarts with a fresh MAC and no bonds. Ducky Scripts / Custom Gestures / pinned launches don't stop BLE either. Use
   `bt_remotes_profile_clear_pairing` to wipe bonding data — it only touches the key files, so it's
   safe whether BLE is running or stopped. **Per-profile `delay_connect`:** when set, the Start scene owns BLE — `on_event` starts
   it on the way into any non-Settings destination, `on_enter` stops it on every return to the menu;
   the Settings-Back / `unpair` / `reset_profile` (registered, but currently unreachable) auto-restarts go through
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
- **`views/hid_remote_menu.c`** — the reorderable/pinnable Start-menu view. It and
  `views/hid_ptt_menu.c` read `momentum_settings.wrap_on_hold` (Momentum's "List Wraparound"
  setting, added by a carried firmware commit) to gate wrap-at-extremes during hold-scroll.
- **`helpers/`** — `ble_hid_ext_profile` (custom MAC/name), `ducky_runner`, `gesture_runner`.

When you change persistence formats, menu indices, or the BLE lifecycle, update
`docs/ARCHITECTURE.md` to match.

---

## Agent skills

### Issue tracker

GitHub Issues on `Skordio/flipper-bt-remotes` via `gh`; always pass `-R Skordio/flipper-bt-remotes`. See `docs/agents/issue-tracker.md`.

### Triage labels

Default five roles (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`), plus `bug` / `enhancement` categories. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: `CONTEXT.md` + `docs/adr/` at this app's root, created lazily. See `docs/agents/domain.md`.
