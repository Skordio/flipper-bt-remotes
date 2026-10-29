# BT Remotes

> 🤖 **AI was used heavily in the development of this app.** If that matters to you, take it into
> account when using the app and reviewing the code.

Turn your Flipper Zero into a **Bluetooth remote** for your computer, phone, or TV. BT Remotes
makes the Flipper act as a Bluetooth keyboard, mouse, media controller, and more — with multiple
saved profiles so it can remember several paired devices and switch between them instantly.

> ⚠️ **Momentum firmware only.** This app is built for
> [Momentum firmware](https://momentum-fw.dev/) and won't install on other firmware — on stock,
> Unleashed, or RogueMaster the Flipper shows *"Update Firmware to use with this Application"*.
> See [Compatibility](#compatibility).

---

## Features

- **Multiple profiles** — each profile has its own Bluetooth identity (address + name) and its
  own saved pairing, so you can keep separate remotes for your laptop, phone, and TV and switch
  between them without re-pairing.
- **Includes all remotes from stock fw bluetooth remote**
- **iOS Phone remote** — a mouse remote tuned for an iPhone: hold a d-pad direction to move the
  cursor (constant speed or ramping up), double-tap to swipe, or switch to Swipe mode for
  single-press swipes.
- **Ducky Scripts** — minimal duckyscript runner allows you to run duckyscripts without leaving
  the BT Remotes app.
- **Custom Gestures** — a simple built-in scripting language for creating a sequence of mouse
  moves, clicks, drags, waits, and keystrokes (e.g. "open Settings and toggle a switch"), then
  executing it on a device.
- **Customizable menu** — reorder the main menu, hide remotes you don't use, and **pin** your
  favorite Ducky collections or gestures to the menu for one-tap access.
- **Per-remote settings** — tune behavior like the Keynote back-button key, Media remote mode,
  TikTok swipe distances, iOS Phone cursor and swipe speeds, and whether Ducky scripts connect
  only while they run.
- **Delay Connect** — an optional per-profile mode that keeps Bluetooth off at the menu and
  turns it on only while a remote, script, or gesture is open.
- **Connection feedback** — optional vibration on connect/disconnect, and a blue LED while
  connected.

---

## Compatibility

| Firmware | Supported? |
|---|---|
| **Momentum** | ✅ Yes — required |
| Official (stock) | ❌ Won't install ("Update Firmware to use with this Application") |
| Other forks (Unleashed, RogueMaster, etc.) | ❌ Won't install |

BT Remotes is built against Momentum, and relies on a few app-facing resources (some built-in
icons and UI helpers) that Momentum makes available to apps but other firmwares don't. Because of
that, other firmwares won't load it.

Two features also depend on small firmware patches that currently live in the author's
[Momentum fork](https://github.com/Skordio/Momentum-Firmware): opening `.btremote` shortcuts from
the File Browser, and the **List Wraparound** setting (Momentum app → Interface → General) that
the Start, Profile Select, and PushToTalk lists follow. On a Momentum build without them the app
still loads, but `.btremote` files won't launch it and the lists read a setting that isn't there,
so their wrap-around at the ends isn't reliable. Building from source needs the fork too (the
menu code reads `momentum_settings.wrap_on_hold`).

---

## Installing

1. Make sure your Flipper is running **Momentum firmware** (via the Momentum app or
   [momentum-fw.dev](https://momentum-fw.dev/)).
2. Get `bt_remotes.fap`:
   - Download it from the app's releases, **or**
   - Build it from source (see below).
3. Copy `bt_remotes.fap` to your Flipper at `SD Card / apps / Bluetooth /` (using qFlipper or
   the Momentum app).
4. On the Flipper, open **Apps → Bluetooth → BT Remotes**.

### Building from source

This app lives in the Momentum firmware tree. From the firmware root:

```sh
./fbt launch APPSRC=applications_user/bt_remotes      # build + install + run on a connected Flipper
./fbt build  APPSRC=applications_user/bt_remotes      # just build the .fap
```

(On Windows use `.\fbt.cmd` instead of `./fbt`.) The tree must include the firmware patch
described under [Compatibility](#compatibility).

---

## Using it

1. **Create a profile.** On first launch, make a new profile and give it a name. This becomes a
   distinct Bluetooth device your computer/phone will see.
2. **Pair.** Open any remote (e.g. Keyboard), then on your computer/phone go to Bluetooth
   settings and pair with the Flipper. The LED turns blue when connected.
3. **Use a remote.** Pick a remote from the menu and control your device.
4. **Switch devices.** Back out to the profile list and pick a different profile to act as a
   different Bluetooth device — each remembers its own pairing.

**Tips**
- **Reorder / hide / pin:** In a profile's menu, hold **OK** on an item to reorder it (**OK** or
  **Back** drops it). **Settings → Start Menu Layout** has **Hide Remote Types** and **Reset Menu
  Order**. Pin a Ducky collection or a gesture from its **Pin to Start** option.
- **Leaving:** hold **Back** to exit a remote to its menu. Holding Back for several seconds
  (Profile Select → **Settings** → **Hold Back Quit**, 1–10 s, default 6) quits the app from
  anywhere.
- **Ducky Scripts:** Main menu → **Ducky Scripts** → **Browse Files** to run a `.txt` script, or
  **Collections** to group several.
- **Custom Gestures:** write a `.gesture` text file on your computer (see
  [`docs/WRITING_GESTURES.md`](docs/WRITING_GESTURES.md)) and copy it to `gestures/` on the SD
  card (below). Then Main menu → **Custom Gestures** → pick it → **Run** or **Pin to Start**.
- **Launch a profile directly:** In a profile, go to **Settings → Profile Management → Create
  Shortcut**, then pick **Profile Only**, a specific remote, or one of the profile's pinned
  collections/gestures. This writes a `.btremote` file to `apps_data/bt_remotes/launchers/`.
  Open it from the Flipper's File Browser (or pin it to Favorites) and the app boots straight
  into that profile's menu — or all the way into the picked remote / pinned item — no Profile
  Select. Move the `.btremote` anywhere on the SD card; it still works. Requires the firmware
  patch described under [Compatibility](#compatibility).

### Where files are stored

On the SD card under `apps_data/bt_remotes/`:

- `app.cfg` — global settings (default BT name, vibration, Hold Back Quit, profile order).
- `profiles/` — your saved profiles (each `.cfg` + `.keys` + optional `.pins`).
- `collections/` — saved Ducky Script collections (`.collection`).
- `gestures/` — your Custom Gestures (`.gesture`). These are global (shared by all profiles) and
  are plain text you can edit on a computer. The app doesn't create this folder — make it
  yourself before copying gestures in.
- `launchers/` — profile-launcher shortcuts (`.btremote`), one per profile you chose to make a
  shortcut for.

---

## Notes

- Profiles use a randomized static Bluetooth address so each one looks like a separate device.
- **Settings → Connection → Bluetooth Unpairing** clears a profile's saved pairing (and gives it
  a new Bluetooth address) if a host gets confused.

---

For developer/architecture documentation, see [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).
