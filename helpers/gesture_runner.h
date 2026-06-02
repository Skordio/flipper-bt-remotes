#pragma once

#include <furi.h>
#include <ble_profile/extra_profiles/hid_profile.h>

// Custom Gestures runner: executes a tiny, gesture-focused language (lowercase
// verbs) against a BLE HID profile on a worker thread. This is a SEPARATE engine
// from helpers/ducky_runner.c — it shares only the threading/stop/popup PATTERN,
// not the DuckyScript vocabulary.
//
// Language (one command per line; '#' and blank lines ignored; verbs are
// case-insensitive):
//   anchor tl|tr|bl|br   slam cursor to a screen corner (repeatable origin)
//   move <dx> <dy>       relative mouse move (px), auto-chunked into int8 steps
//   tap                  finger tap = left press + release
//   click left|right     mouse button click
//   drag <dx> <dy>       left press -> stepped move -> release
//   scroll <n>           mouse wheel (signed)
//   wait <ms>            delay
//   key <combo>          keyboard key/combo, e.g. "key enter", "key cmd space"
//   type <text>          type a literal string
//   run <name>           run another gesture by name (script inheritance)

// On-disk format (FlipperFormat). Shared by the runner (reads) and the library
// save/load in bt_remotes.c (writes), so they must agree.
#define GESTURE_FILE_TYPE    "Flipper BT Custom Gesture"
#define GESTURE_FILE_VERSION (1)
#define GESTURE_LINE_MAX     64 // max gesture lines per file
#define GESTURE_LINE_LEN     64 // max chars per command line (incl. NUL)

typedef enum {
    GestureRunnerStateIdle,
    GestureRunnerStateRunning,
    GestureRunnerStateDone,
    GestureRunnerStateError,
} GestureRunnerState;

typedef struct GestureRunner GestureRunner;

// Called from the worker thread when state changes (done / error). Use
// view_dispatcher_send_custom_event inside the callback to forward to main thread.
typedef void (*GestureRunnerCallback)(void* context);

GestureRunner* gesture_runner_alloc(void);
void           gesture_runner_free(GestureRunner* runner);

// Start executing the gesture file at |path| through the supplied BLE HID profile.
// Returns false if a gesture is already running.
bool gesture_runner_start(
    GestureRunner*         runner,
    FuriHalBleProfileBase* profile,
    const char*            path);

// Stop a running gesture (safe to call even if already stopped/done).
void gesture_runner_stop(GestureRunner* runner);

GestureRunnerState gesture_runner_get_state(const GestureRunner* runner);

// Human-readable error string — valid only when state == GestureRunnerStateError.
const char* gesture_runner_get_error(const GestureRunner* runner);

void gesture_runner_set_callback(GestureRunner* runner, GestureRunnerCallback cb, void* context);

// Validate a single gesture command line (used by the on-device line editor).
// Returns true if the line parses; on failure writes a short reason into
// |err| (size |err_size|). Blank lines and comments are considered valid.
bool gesture_line_validate(const char* line, char* err, size_t err_size);
