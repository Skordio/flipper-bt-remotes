# Writing Custom Gestures

**Custom Gestures** is a tiny scripting language in BT Remotes for recording a sequence of mouse
moves, clicks, drags, waits, and keystrokes — a reusable "quick action" you can replay with one
button press. Typical use: navigate deep into a host's UI (e.g. an iOS Settings screen) and flip
a switch, hands-free.

This guide is for writing gestures by hand. You can also build them on the device
(**Custom Gestures → + Create**), but editing the plain-text files on a computer is often faster.

---

## File format

A `.gesture` file is plain text: **one command per line**.

```
anchor bl
move 100 0
press
move 0 -550
release
```

- **No header is needed** — just list the commands.
- **Blank lines** are ignored.
- **Comments** start with `#` and run to the end of the line.
- A legacy `Filetype:` / `Version:` header (written by older versions of the app)
  is still accepted and ignored, so old files keep working — but you don't need it.

```
# Go to the Home screen
anchor bl       # plant a known origin at the bottom-left
move 100 0      # step in off the corner
```

Limits: up to **64 commands** per file, **63 characters** per line. Verbs are
case-insensitive (`MOVE`, `move`, and `Move` are the same).

### Where files live

- **On the Flipper:** `/ext/apps_data/bt_remotes/gestures/` (the SD card). The filename without
  `.gesture` is the gesture's name in the app. Gestures are **global** — shared across all
  profiles.
- **In this repo:** ready-made examples are in [`../examples/`](../examples/).

Upload from a computer with the Flipper CLI / `storage.py`:

```
python scripts/storage.py -p <PORT> send home.gesture /ext/apps_data/bt_remotes/gestures/home.gesture
```

---

## How it works (the mental model)

The Flipper acts as a **Bluetooth mouse + keyboard**. The mouse is **relative**: every move is an
offset from wherever the cursor currently is, in pixels. There's no "move to (x, y)" — only "move
by (dx, dy)".

To make moves repeatable, start from a known **origin** using `anchor`, which slams the cursor
hard into a screen corner. After anchoring, a `move` is effectively measured from that corner, so
the same script lands in the same place every run.

Large moves are split automatically into small steps under the hood, so you can write
`move 0 -550` and not worry about packet sizes.

---

## Command reference

| Command | What it does |
|---|---|
| `anchor tl\|tr\|bl\|br` | Slam the cursor to a screen corner (top-left, top-right, bottom-left, bottom-right). Use as a repeatable origin before relative moves. |
| `move <dx> <dy>` | Move the cursor by `dx` right / `dy` down (negative = left / up), in pixels. |
| `tap` | A quick left press + release (a "click"/touch where the cursor is). |
| `click left\|right` | A left or right button click. |
| `drag <dx> <dy>` | Press left, move by `dx,dy`, release — all in one step (a quick swipe/drag). |
| `press [left\|right]` | Press a button **and hold it** (default left). Pair with `move` and `release` for a drag whose timing you control. |
| `release [left\|right]` | Release a held button (default left). |
| `scroll <n>` | Mouse wheel; positive/negative for direction. |
| `wait <ms>` | Pause for `ms` milliseconds (lets the host UI catch up / animate). |
| `key <combo>` | Send a keyboard key or combo (see below). |
| `type <text>` | Type a literal string. |
| `run <name>` | Run another gesture by name — reuse a gesture as a building block (see *Reusing gestures*). |

### `drag` vs `press`/`move`/`release`

`drag 0 -550` is the quick version: press, swipe, release with fixed short holds. When you need
to control the timing — hold at the start, swipe slowly, pause at the end — use the manual form:

```
press            # button down (and stays down)
wait 200         # dwell before moving
move 0 -550      # swipe up while held
wait 100         # settle at the end
release          # button up
```

### `key` combos

`key <combo>` accepts space-separated tokens: zero or more modifiers plus one key.

- **Modifiers:** `ctrl` (`control`), `shift`, `alt`, `gui` (`cmd` / `win`).
- **Named keys:** `enter` (`return`), `tab`, `space`, `backspace`, `delete`, `esc` (`escape`),
  `up`, `down`, `left`, `right`, `home`, `end`.
- **Any single character:** e.g. `a`, `7`, `/`.

Examples:

```
key enter
key cmd space      # e.g. Spotlight / search
key ctrl shift t
```

For typing words or sentences, prefer `type`:

```
type settings
```

---

## Reusing gestures (`run`)

`run <name>` executes another gesture file (by name, from the same folder) and then continues.
This lets you factor out common building blocks. For example, `home.gesture` does the "go to the
Home screen" sequence, and other gestures start with:

```
run home
wait 1000
# ...continue from a known starting point...
```

Notes:
- The referenced gesture must be in the same `gestures/` folder.
- Nesting is allowed up to **5 levels deep** (a guard against accidental loops). A `run` of a
  missing gesture is skipped; deeper-than-5 nesting is ignored.
- Avoid cycles (A runs B runs A) — the depth limit stops them, but they won't do anything useful.

---

## Authoring tips

- **Anchor first.** Begin with `anchor` so your moves are repeatable. Without it, the cursor
  starts wherever it last was and the script drifts.
- **Tune in small steps.** Move values are device/screen-specific. Get one move landing right,
  then add the next. Because gestures are plain files, you can tweak a number and re-upload in
  seconds — no rebuild needed.
- **Give the UI time.** Add `wait` after actions that open menus, animate, or load. Too fast and
  the next move lands before the screen is ready.
- **Comment your intent.** `# open Settings` lines make a long gesture readable later.
- **Build big gestures from small ones.** Get `home` solid, then `run home` everywhere.

---

## Host-specific notes

### iOS / iPadOS (important)

Driving an iPhone/iPad with a Bluetooth mouse goes through **AssistiveTouch** (Settings →
Accessibility → Touch → AssistiveTouch), which is **required** for mouse support on iPhone. A few
things behave differently than you'd expect:

- **The mouse is a pointer, not a finger.** iOS shows a circular pointer that *clicks* — it does
  **not** reproduce touchscreen edge gestures the way a finger does. A "swipe up from the bottom
  to go Home" may not work the same as on a touchscreen.
- **Screen corners are special ("Hot Corners").** If Dwell Control is on, holding the pointer
  **still in a corner** triggers a Dwell action (screenshot, Control Center, Siri, …). That makes
  the `anchor` trick risky on iOS: **don't `press` + `wait` while parked in a corner**, or you'll
  fire Dwell instead of your gesture. Step in off the corner (`move`) before any hold, and keep
  swipes moving rather than dwelling.
- **Reliable "Home" options:** map a mouse button to **Home** in
  AssistiveTouch → Devices → (your mouse) → customize a button → *Home*, then `home.gesture` is
  just `click right` (or whichever button). On iPad, the keyboard route `key cmd h` (or Globe/Fn +
  H) also goes Home.

### Desktop (macOS / Windows / Linux)

The corner-`anchor` + relative-`move` approach works well — there's no AssistiveTouch/Dwell layer
to fight. Use `key cmd space` / `key gui ...` etc. for shortcuts.

---

## Examples

### `home.gesture` — go to the iOS Home screen (pointer edge-swipe)

```
anchor bl       # origin at bottom-left
move 100 0      # step in off the corner (avoid the Dwell hot corner)
press           # touch down at the bottom edge
move 0 -550     # swipe up
release
```

### `head_tracking.gesture` — open Settings, reusing `home`

```
run home        # start from the Home screen
wait 1000
anchor tl       # re-anchor top-left for repeatable Settings navigation
move 200 250
drag 0 300      # pull down to reveal search
wait 800
type settings
wait 600
key enter
wait 2000
anchor tl
move 200 450
tap
```

(These ship in [`../examples/`](../examples/). The exact move values are screen-specific — expect
to tune them for your device.)

---

## Troubleshooting

- **Nothing happens:** make sure the Flipper is connected/paired (blue LED) and the host is
  focused on the right screen. On iPhone, confirm AssistiveTouch is on.
- **The cursor drifts / lands in the wrong place:** add an `anchor` at the start, and re-check
  move values — they're relative and screen-specific.
- **A step happens before the screen is ready:** add a `wait` before it.
- **A line is ignored:** unknown verbs/keys are skipped when running (and rejected in the on-device
  editor). Re-check spelling against the command reference above.
- **iOS triggers Dwell / a screenshot / Control Center:** you're dwelling in a corner — step off
  the corner before any hold, and don't `press` + `wait` while parked in one.

---

For the language's implementation (parser, runner, timing internals), see
[`ARCHITECTURE.md`](ARCHITECTURE.md) → *Custom Gestures*.
