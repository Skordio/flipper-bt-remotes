#include "gesture_runner.h"
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <storage/storage.h>
#include <ble_profile/extra_profiles/hid_profile.h>
#include <ctype.h>

#define TAG "GestureRunner"

// Worker thread stop flag
#define GESTURE_EVT_STOP (1u << 0)

// Per-move chunk + pacing (mirrors views/hid_tiktok.c proven values).
#define GESTURE_MOVE_CHUNK   90 // px per int8-safe step
#define GESTURE_MOVE_DELAY   8 // ms between move packets
#define GESTURE_ANCHOR_STEPS 8 // corner-slam repetitions
#define GESTURE_ANCHOR_DELAY 40 // ms between anchor packets

// Max nesting depth for `run <name>` (script inheritance). Also bounds runaway
// recursion from a gesture that runs itself directly or via a cycle.
#define GESTURE_MAX_DEPTH 5

// ---------------------------------------------------------------------------
// Key lookup (named keys + modifiers) for the `key` command
// ---------------------------------------------------------------------------

typedef struct {
    const char* name;
    uint16_t    keycode;
} GestureKeyEntry;

static const GestureKeyEntry gesture_modifiers[] = {
    {"ctrl", KEY_MOD_LEFT_CTRL},
    {"control", KEY_MOD_LEFT_CTRL},
    {"shift", KEY_MOD_LEFT_SHIFT},
    {"alt", KEY_MOD_LEFT_ALT},
    {"gui", KEY_MOD_LEFT_GUI},
    {"cmd", KEY_MOD_LEFT_GUI},
    {"win", KEY_MOD_LEFT_GUI},
};

static const GestureKeyEntry gesture_named_keys[] = {
    {"enter", HID_KEYBOARD_RETURN},
    {"return", HID_KEYBOARD_RETURN},
    {"tab", HID_KEYBOARD_TAB},
    {"space", HID_KEYBOARD_SPACEBAR},
    {"backspace", HID_KEYBOARD_DELETE},
    {"delete", HID_KEYBOARD_DELETE_FORWARD},
    {"esc", HID_KEYBOARD_ESCAPE},
    {"escape", HID_KEYBOARD_ESCAPE},
    {"up", HID_KEYBOARD_UP_ARROW},
    {"down", HID_KEYBOARD_DOWN_ARROW},
    {"left", HID_KEYBOARD_LEFT_ARROW},
    {"right", HID_KEYBOARD_RIGHT_ARROW},
    {"home", HID_KEYBOARD_HOME},
    {"end", HID_KEYBOARD_END},
};

// ---------------------------------------------------------------------------
// Struct
// ---------------------------------------------------------------------------

struct GestureRunner {
    FuriThread*                thread;
    FuriHalBleProfileBase*     profile;
    char                       path[256];
    char                       error[64];
    volatile GestureRunnerState state;
    volatile bool              user_stopped;
    GestureRunnerCallback      callback;
    void*                      callback_context;
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Interruptible delay: returns false if the stop flag fired, true on normal expiry.
static bool gesture_delay(uint32_t ms) {
    while(ms > 0) {
        uint32_t chunk = ms > 100 ? 100 : ms;
        uint32_t flags = furi_thread_flags_wait(GESTURE_EVT_STOP, FuriFlagWaitAny, chunk);
        if((flags & FuriFlagError) == 0) return false; // stop flag received
        ms -= chunk;
    }
    return true;
}

static bool gesture_should_stop(void) {
    uint32_t flags = furi_thread_flags_get();
    if(flags & GESTURE_EVT_STOP) {
        furi_thread_flags_clear(GESTURE_EVT_STOP);
        return true;
    }
    return false;
}

// Read one text line (strips \r\n, NUL-terminates). Returns false at EOF.
static bool gesture_read_line(File* file, char* buf, size_t buf_size) {
    size_t i = 0;
    char   c;
    bool   got_any = false;
    while(i < buf_size - 1) {
        if(storage_file_read(file, &c, 1) != 1) break;
        got_any = true;
        if(c == '\n') break;
        if(c == '\r') continue;
        buf[i++] = c;
    }
    buf[i] = '\0';
    return got_any;
}

// Lowercase a token in place (for case-insensitive verb/keyword matching).
static void gesture_tolower(char* s) {
    for(; *s; s++) *s = (char)tolower((unsigned char)*s);
}

// Emit a single-axis move of `total` px (signed), chunked to int8 steps.
static bool gesture_move_axis(GestureRunner* runner, bool horizontal, int total) {
    while(total != 0) {
        int step = total;
        if(step > GESTURE_MOVE_CHUNK) step = GESTURE_MOVE_CHUNK;
        if(step < -GESTURE_MOVE_CHUNK) step = -GESTURE_MOVE_CHUNK;
        if(horizontal) {
            ble_profile_hid_mouse_move(runner->profile, (int8_t)step, 0);
        } else {
            ble_profile_hid_mouse_move(runner->profile, 0, (int8_t)step);
        }
        total -= step;
        if(total != 0 && !gesture_delay(GESTURE_MOVE_DELAY)) return false;
    }
    return true;
}

// Move both axes: horizontal component first, then vertical. Returns false on stop.
static bool gesture_move_xy(GestureRunner* runner, int dx, int dy) {
    if(!gesture_move_axis(runner, true, dx)) return false;
    if(dx != 0 && dy != 0) {
        if(!gesture_delay(GESTURE_MOVE_DELAY)) return false;
    }
    return gesture_move_axis(runner, false, dy);
}

// Resolve a single key token to a HID keycode (modifier high byte, key low byte).
static uint16_t gesture_lookup_key(const char* token) {
    for(size_t i = 0; i < COUNT_OF(gesture_modifiers); i++) {
        if(strcmp(token, gesture_modifiers[i].name) == 0) return gesture_modifiers[i].keycode;
    }
    for(size_t i = 0; i < COUNT_OF(gesture_named_keys); i++) {
        if(strcmp(token, gesture_named_keys[i].name) == 0) return gesture_named_keys[i].keycode;
    }
    if(token[0] != '\0' && token[1] == '\0' && (uint8_t)token[0] < 128)
        return HID_ASCII_TO_KEY((uint8_t)token[0]) & 0x00FF;
    return 0;
}

// Parse "ctrl shift a" / "cmd space" / "enter" into a combined keycode.
static uint16_t gesture_parse_combo(const char* args) {
    uint16_t combo = 0;
    char     tok[24];
    const char* p = args;
    while(*p) {
        while(*p == ' ') p++;
        if(!*p) break;
        size_t n = 0;
        while(*p && *p != ' ' && n < sizeof(tok) - 1) tok[n++] = *p++;
        tok[n] = '\0';
        gesture_tolower(tok);
        combo |= gesture_lookup_key(tok);
    }
    return combo;
}

static void gesture_type_string(GestureRunner* runner, const char* text) {
    for(; *text; text++) {
        uint16_t keycode = HID_ASCII_TO_KEY((uint8_t)*text);
        if(keycode != HID_KEYBOARD_NONE) {
            ble_profile_hid_kb_press(runner->profile, keycode);
            furi_delay_ms(2);
            ble_profile_hid_kb_release(runner->profile, keycode);
        }
        if(!gesture_delay(5)) return;
    }
}

// ---------------------------------------------------------------------------
// Line parsing — shared by the runner and the editor validator
// ---------------------------------------------------------------------------

typedef enum {
    GCmdNop, // blank / comment
    GCmdAnchor,
    GCmdMove,
    GCmdTap,
    GCmdClick,
    GCmdDrag,
    GCmdPress, // mouse button down, held until release (manual drag control)
    GCmdRelease, // mouse button up
    GCmdScroll,
    GCmdWait,
    GCmdKey,
    GCmdType,
    GCmdRun, // run another gesture by name (inheritance)
    GCmdInvalid,
} GestureCmd;

// Parsed representation of one line.
typedef struct {
    GestureCmd cmd;
    int        a; // dx / scroll / wait / corner (0=tl,1=tr,2=bl,3=br) / button (0=left,1=right)
    int        b; // dy
    char       text[GESTURE_LINE_LEN]; // for key/type/run: the argument tail
} GestureParsed;

static void gesture_parse(const char* raw, GestureParsed* out) {
    out->cmd     = GCmdNop;
    out->a       = 0;
    out->b       = 0;
    out->text[0] = '\0';

    const char* line = raw;
    while(*line == ' ' || *line == '\t') line++;
    if(*line == '\0' || *line == '#') return; // blank / comment

    // Split verb
    char verb[12];
    size_t n = 0;
    const char* p = line;
    while(*p && *p != ' ' && n < sizeof(verb) - 1) verb[n++] = *p++;
    verb[n] = '\0';
    gesture_tolower(verb);
    while(*p == ' ') p++; // p now at args

    if(strcmp(verb, "tap") == 0) {
        out->cmd = GCmdTap;
    } else if(strcmp(verb, "anchor") == 0) {
        char c[4];
        size_t m = 0;
        while(*p && *p != ' ' && m < sizeof(c) - 1) c[m++] = *p++;
        c[m] = '\0';
        gesture_tolower(c);
        if(strcmp(c, "tl") == 0)
            out->a = 0;
        else if(strcmp(c, "tr") == 0)
            out->a = 1;
        else if(strcmp(c, "bl") == 0)
            out->a = 2;
        else if(strcmp(c, "br") == 0)
            out->a = 3;
        else {
            out->cmd = GCmdInvalid;
            return;
        }
        out->cmd = GCmdAnchor;
    } else if(strcmp(verb, "move") == 0 || strcmp(verb, "drag") == 0) {
        const char* sp = strchr(p, ' ');
        if(!sp || p[0] == '\0') {
            out->cmd = GCmdInvalid;
            return;
        }
        out->a   = atoi(p);
        out->b   = atoi(sp + 1);
        out->cmd = (verb[0] == 'm') ? GCmdMove : GCmdDrag;
    } else if(strcmp(verb, "click") == 0) {
        char c[8];
        size_t m = 0;
        while(*p && *p != ' ' && m < sizeof(c) - 1) c[m++] = *p++;
        c[m] = '\0';
        gesture_tolower(c);
        if(strcmp(c, "left") == 0)
            out->a = 0;
        else if(strcmp(c, "right") == 0)
            out->a = 1;
        else {
            out->cmd = GCmdInvalid;
            return;
        }
        out->cmd = GCmdClick;
    } else if(strcmp(verb, "press") == 0 || strcmp(verb, "release") == 0) {
        // Optional button arg (default left). Holds/releases the button so a
        // gesture can do a manual drag with its own timing: press / wait / move / release.
        out->a = 0; // 0 = left, 1 = right
        if(*p != '\0') {
            char c[8];
            size_t m = 0;
            while(*p && *p != ' ' && m < sizeof(c) - 1) c[m++] = *p++;
            c[m] = '\0';
            gesture_tolower(c);
            if(strcmp(c, "left") == 0)
                out->a = 0;
            else if(strcmp(c, "right") == 0)
                out->a = 1;
            else {
                out->cmd = GCmdInvalid;
                return;
            }
        }
        out->cmd = (verb[0] == 'p') ? GCmdPress : GCmdRelease;
    } else if(strcmp(verb, "scroll") == 0) {
        if(p[0] == '\0') {
            out->cmd = GCmdInvalid;
            return;
        }
        out->a   = atoi(p);
        out->cmd = GCmdScroll;
    } else if(strcmp(verb, "wait") == 0) {
        if(p[0] == '\0' || atoi(p) < 0) {
            out->cmd = GCmdInvalid;
            return;
        }
        out->a   = atoi(p);
        out->cmd = GCmdWait;
    } else if(strcmp(verb, "key") == 0) {
        if(p[0] == '\0') {
            out->cmd = GCmdInvalid;
            return;
        }
        strlcpy(out->text, p, sizeof(out->text));
        out->cmd = GCmdKey;
    } else if(strcmp(verb, "type") == 0) {
        if(p[0] == '\0') {
            out->cmd = GCmdInvalid;
            return;
        }
        strlcpy(out->text, p, sizeof(out->text));
        out->cmd = GCmdType;
    } else if(strcmp(verb, "run") == 0) {
        if(p[0] == '\0') {
            out->cmd = GCmdInvalid;
            return;
        }
        strlcpy(out->text, p, sizeof(out->text));
        out->cmd = GCmdRun;
    } else {
        out->cmd = GCmdInvalid;
    }
}

// Execute one parsed command. Returns false if execution should stop.
// Note: GCmdRun is NOT handled here — it needs file access and is dispatched in
// gesture_run_file so it can recurse.
static bool gesture_exec(GestureRunner* runner, const GestureParsed* g) {
    switch(g->cmd) {
    case GCmdNop:
    case GCmdInvalid:
    case GCmdRun:
        return true; // skip blanks/comments/invalid; run handled by caller

    case GCmdAnchor: {
        // corner index: 0=tl,1=tr,2=bl,3=br
        int8_t hx = (g->a == 0 || g->a == 2) ? -127 : 127;
        int8_t vy = (g->a == 0 || g->a == 1) ? -127 : 127;
        for(int i = 0; i < GESTURE_ANCHOR_STEPS; i++) {
            ble_profile_hid_mouse_move(runner->profile, hx, vy);
            if(!gesture_delay(GESTURE_ANCHOR_DELAY)) return false;
        }
        return true;
    }

    case GCmdMove:
        return gesture_move_xy(runner, g->a, g->b);

    case GCmdTap:
        ble_profile_hid_mouse_press(runner->profile, HID_MOUSE_BTN_LEFT);
        if(!gesture_delay(25)) return false;
        ble_profile_hid_mouse_release(runner->profile, HID_MOUSE_BTN_LEFT);
        return true;

    case GCmdClick: {
        uint8_t btn = (g->a == 1) ? HID_MOUSE_BTN_RIGHT : HID_MOUSE_BTN_LEFT;
        ble_profile_hid_mouse_press(runner->profile, btn);
        if(!gesture_delay(25)) return false;
        ble_profile_hid_mouse_release(runner->profile, btn);
        return true;
    }

    case GCmdDrag:
        ble_profile_hid_mouse_press(runner->profile, HID_MOUSE_BTN_LEFT);
        if(!gesture_delay(40)) {
            ble_profile_hid_mouse_release(runner->profile, HID_MOUSE_BTN_LEFT);
            return false;
        }
        if(!gesture_move_xy(runner, g->a, g->b)) {
            ble_profile_hid_mouse_release(runner->profile, HID_MOUSE_BTN_LEFT);
            return false;
        }
        if(!gesture_delay(40)) {
            ble_profile_hid_mouse_release(runner->profile, HID_MOUSE_BTN_LEFT);
            return false;
        }
        ble_profile_hid_mouse_release(runner->profile, HID_MOUSE_BTN_LEFT);
        return true;

    case GCmdPress: {
        uint8_t btn = (g->a == 1) ? HID_MOUSE_BTN_RIGHT : HID_MOUSE_BTN_LEFT;
        ble_profile_hid_mouse_press(runner->profile, btn);
        return true;
    }

    case GCmdRelease: {
        uint8_t btn = (g->a == 1) ? HID_MOUSE_BTN_RIGHT : HID_MOUSE_BTN_LEFT;
        ble_profile_hid_mouse_release(runner->profile, btn);
        return true;
    }

    case GCmdScroll:
        ble_profile_hid_mouse_scroll(runner->profile, (int8_t)g->a);
        return true;

    case GCmdWait:
        return gesture_delay((uint32_t)g->a);

    case GCmdKey: {
        uint16_t combo = gesture_parse_combo(g->text);
        if(combo != 0) {
            ble_profile_hid_kb_press(runner->profile, combo);
            if(!gesture_delay(20)) {
                ble_profile_hid_kb_release(runner->profile, combo);
                return false;
            }
            ble_profile_hid_kb_release(runner->profile, combo);
        }
        return true;
    }

    case GCmdType:
        gesture_type_string(runner, g->text);
        return true;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Recursive file executor (supports `run <name>` inheritance)
// ---------------------------------------------------------------------------

// Resolve "run <name>" to a sibling of parent_path: <dir>/<name><ext-of-parent>.
// All gestures live in the same directory, so we reuse the parent's directory and
// extension. Returns false if the result would not fit in `out`.
static bool gesture_resolve_sibling(
    const char* parent_path,
    const char* name,
    char*       out,
    size_t      out_size) {
    const char* slash = strrchr(parent_path, '/');
    const char* fname = slash ? slash + 1 : parent_path;
    const char* dot   = strrchr(fname, '.');
    const char* ext   = dot ? dot : "";
    int dir_len = (int)(fname - parent_path); // includes the trailing slash
    int needed  = snprintf(out, out_size, "%.*s%s%s", dir_len, parent_path, name, ext);
    return needed > 0 && (size_t)needed < out_size;
}

static bool gesture_run_file(GestureRunner* runner, const char* path, int depth) {
    if(depth > GESTURE_MAX_DEPTH) return true; // nesting too deep: ignore this run

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    file    = storage_file_alloc(storage);

    if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        if(depth == 0) {
            runner->state = GestureRunnerStateError;
            strlcpy(runner->error, "Cannot open gesture", sizeof(runner->error));
            return false;
        }
        // A missing nested gesture is skipped rather than aborting the whole run.
        return true;
    }

    // .gesture files are plain text: one command per line. No header is required;
    // any legacy "Filetype:"/"Version:" header parses as invalid lines and is skipped.
    char line[GESTURE_LINE_LEN];
    bool keep_going = true;

    while(keep_going && gesture_read_line(file, line, sizeof(line))) {
        if(gesture_should_stop()) {
            keep_going = false;
            break;
        }

        GestureParsed parsed;
        gesture_parse(line, &parsed);

        if(parsed.cmd == GCmdRun) {
            char child[256];
            if(gesture_resolve_sibling(path, parsed.text, child, sizeof(child))) {
                keep_going = gesture_run_file(runner, child, depth + 1);
            }
        } else {
            keep_going = gesture_exec(runner, &parsed);
        }

        if(!keep_going) break;
        if(gesture_should_stop()) {
            keep_going = false;
            break;
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return keep_going;
}

// ---------------------------------------------------------------------------
// Worker thread
// ---------------------------------------------------------------------------

static int32_t gesture_worker(void* context) {
    GestureRunner* runner = context;

    runner->state = GestureRunnerStateRunning;
    if(runner->callback) runner->callback(runner->callback_context);

    gesture_run_file(runner, runner->path, 0);

    // Release everything that might be held
    ble_profile_hid_kb_release_all(runner->profile);
    ble_profile_hid_mouse_release_all(runner->profile);
    ble_profile_hid_consumer_key_release_all(runner->profile);

    if(runner->state == GestureRunnerStateError) {
        if(runner->callback) runner->callback(runner->callback_context);
    } else if(runner->state == GestureRunnerStateRunning && !runner->user_stopped) {
        runner->state = GestureRunnerStateDone;
        if(runner->callback) runner->callback(runner->callback_context);
    } else {
        // user_stopped: reset so gesture_runner_start can be called again.
        // Without this, state stays Running and the start guard permanently
        // refuses subsequent runs for the lifetime of the process.
        runner->state = GestureRunnerStateIdle;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

GestureRunner* gesture_runner_alloc(void) {
    GestureRunner* runner = malloc(sizeof(GestureRunner));
    memset(runner, 0, sizeof(GestureRunner));
    runner->state = GestureRunnerStateIdle;
    return runner;
}

void gesture_runner_free(GestureRunner* runner) {
    furi_assert(runner);
    gesture_runner_stop(runner);
    free(runner);
}

bool gesture_runner_start(
    GestureRunner*         runner,
    FuriHalBleProfileBase* profile,
    const char*            path) {
    furi_assert(runner);
    furi_assert(profile);
    furi_assert(path);

    if(runner->state == GestureRunnerStateRunning) return false;

    runner->profile      = profile;
    runner->user_stopped = false;
    strlcpy(runner->path, path, sizeof(runner->path));
    runner->state    = GestureRunnerStateIdle;
    runner->error[0] = '\0';

    if(runner->thread) {
        furi_thread_join(runner->thread);
        furi_thread_free(runner->thread);
    }

    // 4 KB stack: gesture_run_file recurses up to GESTURE_MAX_DEPTH for `run`.
    runner->thread = furi_thread_alloc_ex("GestureRunner", 4096, gesture_worker, runner);
    furi_thread_start(runner->thread);
    return true;
}

void gesture_runner_stop(GestureRunner* runner) {
    furi_assert(runner);
    if(runner->thread == NULL) return;
    runner->user_stopped = true;
    furi_thread_flags_set(furi_thread_get_id(runner->thread), GESTURE_EVT_STOP);
    furi_thread_join(runner->thread);
    furi_thread_free(runner->thread);
    runner->thread = NULL;
}

GestureRunnerState gesture_runner_get_state(const GestureRunner* runner) {
    furi_assert(runner);
    return runner->state;
}

const char* gesture_runner_get_error(const GestureRunner* runner) {
    furi_assert(runner);
    return runner->error;
}

void gesture_runner_set_callback(GestureRunner* runner, GestureRunnerCallback cb, void* context) {
    furi_assert(runner);
    runner->callback         = cb;
    runner->callback_context = context;
}
