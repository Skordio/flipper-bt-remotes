# BT Remotes

Turn your Flipper Zero into a **Bluetooth remote** for your computer, phone, or TV. BT Remotes
makes the Flipper act as a Bluetooth keyboard, mouse, media controller, and more — with multiple
saved profiles so it can remember several paired devices and switch between them instantly.

> ⚠️ **Momentum firmware only.** This app uses Bluetooth features that exist only in
> [Momentum firmware](https://momentum-fw.dev/). On stock or other firmware it will refuse to
> start (and older builds could crash the Flipper). See [Compatibility](#compatibility).

---

## Features

- **Multiple profiles** — each profile has its own Bluetooth identity (address + name) and its
  own saved pairing, so you can keep separate remotes for your laptop, phone, and TV and switch
  between them without re-pairing.
- **A whole set of remotes:**
  - **Keynote** / **Keynote Vertical** — slideshow clicker (arrows, play, Esc).
  - **Keyboard** & **Numpad** — full on-screen typing.
  - **Media** — play/pause, track, and volume control.
  - **Apple Music (macOS)** — dedicated Apple Music controls.
  - **Movie** — playback controls tuned for video.
  - **TikTok / YT Shorts** — scroll shorts hands-free (wheel or swipe-gesture mode).
  - **Mouse**, **Mouse Clicker**, **Mouse Jiggler**, **Mouse Jiggler Stealth** — pointer control
    and keep-awake jigglers.
  - **PushToTalk** — push-to-talk button for calls/meetings.
- **Ducky Scripts** — browse and run `.txt` DuckyScript files straight from the Flipper, and
  save **Collections** of scripts to run together.
- **Custom Gestures** — a simple built-in scripting language for recording a sequence of mouse
  moves, clicks, drags, waits, and keystrokes (e.g. "open Settings and toggle a switch"), then
  replaying it with one button press. Gestures can even reuse other gestures.
- **Customizable menu** — reorder the main menu, hide remotes you don't use, and **pin** your
  favorite scripts or gestures to the top for one-tap access.
- **Per-remote settings** — tune behavior like the Keynote back-button key, Media remote mode,
  and TikTok swipe distances.
- **Connection feedback** — optional vibration on connect/disconnect, and a blue LED while
  connected.

---

## Compatibility

| Firmware | Supported? |
|---|---|
| **Momentum** | ✅ Yes — required |
| Official (stock) | ❌ No — app refuses to start |
| Other forks (Unleashed, RogueMaster, etc.) | ❌ Not supported |

BT Remotes depends on Momentum's per-profile custom Bluetooth address support, which other
firmwares don't provide. To keep things safe, the app **checks the firmware at startup** and
shows a "Wrong Firmware" message instead of running (older versions could otherwise lock up the
Flipper and need a reboot). If you see that message, flash Momentum firmware and try again.

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

(On Windows use `.\fbt.cmd` instead of `./fbt`.)

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
- **Reorder / hide / pin:** In a profile's menu, long-press or use **Settings → Hide Items** and
  the menu's reorder mode to arrange things; pin a script or gesture from its **Pin to Start**
  option.
- **Ducky Scripts:** Main menu → **Ducky Scripts** → **Browse Files** to run a `.txt` script, or
  **Collections** to group several.
- **Custom Gestures:** Main menu → **Custom Gestures** → create one, add command lines (there's a
  built-in Help page listing every command), then run or pin it.

### Where files are stored

On the SD card under `apps_data/bt_remotes/`:

- `profiles/` — your saved profiles (each `.cfg` + `.keys` + optional `.pins`).
- `collections/` — saved Ducky Script collections (`.collection`).
- `gestures/` — your Custom Gestures (`.gesture`). These are global (shared by all profiles) and
  are plain text you can edit on a computer.

---

## Notes

- Profiles use a randomized static Bluetooth address so each one looks like a separate device.
- "Unpair" / "Reset" in a profile's Settings clear the saved pairing if a host gets confused.

---

## A note on development

AI was used heavily in the development of this app. If that matters to you, take it into account
when using the app and reviewing the code.

---

For developer/architecture documentation, see [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md).
