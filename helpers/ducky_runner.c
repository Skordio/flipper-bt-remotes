#include "ducky_runner.h"
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <storage/storage.h>
#include <ble_profile/extra_profiles/hid_profile.h>

#define TAG "DuckyRunner"

// Worker thread stop flag
#define DUCKY_EVT_STOP (1u << 0)

// Maximum characters per script line (the script file is read one byte at a time)
#define DUCKY_LINE_MAX 512

// ---------------------------------------------------------------------------
// Key lookup tables
// ---------------------------------------------------------------------------

typedef struct {
    const char* name;
    uint16_t    keycode;
} DuckyKeyEntry;

static const DuckyKeyEntry ducky_modifiers[] = {
    {"CTRL",    KEY_MOD_LEFT_CTRL},
    {"CONTROL", KEY_MOD_LEFT_CTRL},
    {"SHIFT",   KEY_MOD_LEFT_SHIFT},
    {"ALT",     KEY_MOD_LEFT_ALT},
    {"GUI",     KEY_MOD_LEFT_GUI},
    {"WINDOWS", KEY_MOD_LEFT_GUI},
};

static const DuckyKeyEntry ducky_named_keys[] = {
    {"ENTER",       HID_KEYBOARD_RETURN},
    {"RETURN",      HID_KEYBOARD_RETURN},
    {"TAB",         HID_KEYBOARD_TAB},
    {"SPACE",       HID_KEYBOARD_SPACEBAR},
    {"BACKSPACE",   HID_KEYBOARD_DELETE},
    {"DELETE",      HID_KEYBOARD_DELETE_FORWARD},
    {"ESC",         HID_KEYBOARD_ESCAPE},
    {"ESCAPE",      HID_KEYBOARD_ESCAPE},
    {"UP",          HID_KEYBOARD_UP_ARROW},
    {"UPARROW",     HID_KEYBOARD_UP_ARROW},
    {"DOWN",        HID_KEYBOARD_DOWN_ARROW},
    {"DOWNARROW",   HID_KEYBOARD_DOWN_ARROW},
    {"LEFT",        HID_KEYBOARD_LEFT_ARROW},
    {"LEFTARROW",   HID_KEYBOARD_LEFT_ARROW},
    {"RIGHT",       HID_KEYBOARD_RIGHT_ARROW},
    {"RIGHTARROW",  HID_KEYBOARD_RIGHT_ARROW},
    {"HOME",        HID_KEYBOARD_HOME},
    {"END",         HID_KEYBOARD_END},
    {"PAGEUP",      HID_KEYBOARD_PAGE_UP},
    {"PAGEDOWN",    HID_KEYBOARD_PAGE_DOWN},
    {"INSERT",      HID_KEYBOARD_INSERT},
    {"PRINTSCREEN", HID_KEYBOARD_PRINT_SCREEN},
    {"CAPSLOCK",    HID_KEYBOARD_CAPS_LOCK},
    {"NUMLOCK",     HID_KEYPAD_NUMLOCK},
    {"SCROLLLOCK",  HID_KEYBOARD_SCROLL_LOCK},
    {"PAUSE",       HID_KEYBOARD_PAUSE},
    {"BREAK",       HID_KEYBOARD_PAUSE},
    {"MENU",        HID_KEYBOARD_APPLICATION},
    {"APP",         HID_KEYBOARD_APPLICATION},
    {"F1",          HID_KEYBOARD_F1},
    {"F2",          HID_KEYBOARD_F2},
    {"F3",          HID_KEYBOARD_F3},
    {"F4",          HID_KEYBOARD_F4},
    {"F5",          HID_KEYBOARD_F5},
    {"F6",          HID_KEYBOARD_F6},
    {"F7",          HID_KEYBOARD_F7},
    {"F8",          HID_KEYBOARD_F8},
    {"F9",          HID_KEYBOARD_F9},
    {"F10",         HID_KEYBOARD_F10},
    {"F11",         HID_KEYBOARD_F11},
    {"F12",         HID_KEYBOARD_F12},
};

// ---------------------------------------------------------------------------
// Struct
// ---------------------------------------------------------------------------

struct DuckyRunner {
    FuriThread*          thread;
    FuriHalBleProfileBase* profile;
    char                 path[256];
    char                 error[64];
    volatile DuckyRunnerState state;
    volatile bool        user_stopped; // set by ducky_runner_stop() to suppress spurious Done event
    uint32_t             default_delay; // ms appended after each command
    uint32_t             string_delay;  // ms between chars in STRING
    DuckyRunnerCallback  callback;
    void*                callback_context;
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Returns false if the stop flag was set (caller should abort).
static bool ducky_delay(uint32_t ms) {
    while(ms > 0) {
        uint32_t chunk = ms > 100 ? 100 : ms;
        uint32_t flags = furi_thread_flags_wait(DUCKY_EVT_STOP, FuriFlagWaitAny, chunk);
        if((flags & FuriFlagError) == 0) {
            // Stop flag received — consume it and signal abort
            return false;
        }
        // Timeout — normal, keep going
        ms -= chunk;
    }
    return true;
}

// Check (without blocking) whether the stop flag has been set.
static bool ducky_should_stop(void) {
    uint32_t flags = furi_thread_flags_get();
    if(flags & DUCKY_EVT_STOP) {
        furi_thread_flags_clear(DUCKY_EVT_STOP);
        return true;
    }
    return false;
}

// Read one text line from the file into buf (strips \r\n, NUL-terminates).
// Returns false when the file is exhausted.
static bool ducky_read_line(File* file, char* buf, size_t buf_size) {
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

// Check whether the word at *p matches name followed by a delimiter.
// If yes, advances *p past the word and returns true.
static bool ducky_match_word(const char** p, const char* name) {
    size_t len = strlen(name);
    if(strncmp(*p, name, len) != 0) return false;
    char after = (*p)[len];
    if(after != ' ' && after != '-' && after != '\0' && after != '\r' && after != '\n') {
        return false;
    }
    *p += len;
    return true;
}

// ---------------------------------------------------------------------------
// Execution helpers
// ---------------------------------------------------------------------------

static void ducky_type_string(DuckyRunner* runner, const char* text) {
    while(*text && *text != '\r' && *text != '\n') {
        uint16_t keycode = HID_ASCII_TO_KEY((uint8_t)*text);
        if(keycode != HID_KEYBOARD_NONE) {
            ble_profile_hid_kb_press(runner->profile, keycode);
            ble_profile_hid_kb_release(runner->profile, keycode);
        }
        text++;
        if(runner->string_delay > 0) {
            if(!ducky_delay(runner->string_delay)) return;
        }
    }
}

// Parse and send a modifier+key combo (everything after a command keyword has
// been stripped; this is called with the raw line for stand-alone key lines).
static void ducky_press_combo(DuckyRunner* runner, const char* line) {
    uint16_t   combo = 0;
    const char* p    = line;

    while(*p && *p != '\r' && *p != '\n') {
        // Skip whitespace / hyphens between tokens
        while(*p == ' ' || *p == '-') p++;
        if(!*p || *p == '\r' || *p == '\n') break;

        // Try each modifier
        bool found_mod = false;
        for(size_t i = 0; i < COUNT_OF(ducky_modifiers); i++) {
            if(ducky_match_word(&p, ducky_modifiers[i].name)) {
                combo    |= ducky_modifiers[i].keycode;
                found_mod = true;
                break;
            }
        }
        if(found_mod) continue;

        // Try named key — once matched, it is the main key; stop parsing
        bool found_key = false;
        for(size_t i = 0; i < COUNT_OF(ducky_named_keys); i++) {
            if(ducky_match_word(&p, ducky_named_keys[i].name)) {
                combo    |= ducky_named_keys[i].keycode;
                found_key = true;
                break;
            }
        }
        if(found_key) break;

        // Single ASCII character — raw keycode only (no implicit shift modifier)
        // so that "CTRL a" sends Ctrl+a, not Ctrl+Shift+a.
        if((uint8_t)*p < 128) {
            uint16_t keycode = HID_ASCII_TO_KEY((uint8_t)*p) & 0x00FF;
            if(keycode != HID_KEYBOARD_NONE) combo |= keycode;
            p++;
        } else {
            p++; // non-ASCII, skip
        }
        break; // single char is always the last token
    }

    if(combo != 0) {
        ble_profile_hid_kb_press(runner->profile, combo);
        ble_profile_hid_kb_release(runner->profile, combo);
    }
}

// Execute one line of DuckyScript.  Returns false if execution should stop.
static bool ducky_exec_line(DuckyRunner* runner, const char* raw) {
    // Trim leading whitespace
    const char* line = raw;
    while(*line == ' ' || *line == '\t') line++;

    // Empty line / comment
    if(*line == '\0' || *line == '\r' || *line == '\n') return true;
    if(strncmp(line, "REM", 3) == 0 && (line[3] == ' ' || line[3] == '\0')) return true;
    if(strncmp(line, "//",  2) == 0) return true;

    // STRING — type text verbatim (including shift for uppercase / symbols)
    if(strncmp(line, "STRINGLN ", 9) == 0) {
        ducky_type_string(runner, line + 9);
        ble_profile_hid_kb_press(runner->profile, HID_KEYBOARD_RETURN);
        ble_profile_hid_kb_release(runner->profile, HID_KEYBOARD_RETURN);
        return true;
    }
    if(strncmp(line, "STRING ", 7) == 0) {
        ducky_type_string(runner, line + 7);
        return true;
    }

    // DELAY <ms>
    if(strncmp(line, "DELAY ", 6) == 0) {
        uint32_t ms = (uint32_t)atoi(line + 6);
        return ducky_delay(ms);
    }

    // DEFAULTDELAY / DEFAULT_DELAY <ms>
    if(strncmp(line, "DEFAULTDELAY ", 13) == 0) {
        runner->default_delay = (uint32_t)atoi(line + 13);
        return true;
    }
    if(strncmp(line, "DEFAULT_DELAY ", 14) == 0) {
        runner->default_delay = (uint32_t)atoi(line + 14);
        return true;
    }

    // STRINGDELAY / STRING_DELAY <ms>
    if(strncmp(line, "STRINGDELAY ", 12) == 0) {
        runner->string_delay = (uint32_t)atoi(line + 12);
        return true;
    }
    if(strncmp(line, "STRING_DELAY ", 13) == 0) {
        runner->string_delay = (uint32_t)atoi(line + 13);
        return true;
    }

    // Commands we intentionally skip (not yet implemented or irrelevant over BLE)
    if(strncmp(line, "REPEAT",         6) == 0) return true;
    if(strncmp(line, "WAITFORBUTTON",  13) == 0) return true;
    if(strncmp(line, "WAIT_FOR_BUTTON",15) == 0) return true;
    if(strncmp(line, "ID ",            3) == 0) return true;
    if(strncmp(line, "BT_ID ",         6) == 0) return true;
    if(strncmp(line, "BLE_ID ",        7) == 0) return true;
    if(strncmp(line, "ATTACKMODE",    10) == 0) return true;
    if(strncmp(line, "LED",            3) == 0) return true;

    // Everything else: treat as a modifier+key combo
    ducky_press_combo(runner, line);
    return true;
}

// ---------------------------------------------------------------------------
// Worker thread
// ---------------------------------------------------------------------------

static int32_t ducky_worker(void* context) {
    DuckyRunner* runner = context;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File*    file    = storage_file_alloc(storage);

    if(!storage_file_open(file, runner->path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        runner->state = DuckyRunnerStateError;
        strlcpy(runner->error, "Cannot open script", sizeof(runner->error));
        FURI_LOG_E(TAG, "Failed to open: %s", runner->path);
        if(runner->callback) runner->callback(runner->callback_context);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        return 0;
    }

    runner->default_delay = 0;
    runner->string_delay  = 0;
    runner->state         = DuckyRunnerStateRunning;
    if(runner->callback) runner->callback(runner->callback_context);

    char line[DUCKY_LINE_MAX];
    bool keep_going = true;

    while(keep_going && ducky_read_line(file, line, sizeof(line))) {
        if(ducky_should_stop()) break;

        keep_going = ducky_exec_line(runner, line);

        if(!keep_going || runner->state == DuckyRunnerStateError) break;
        if(ducky_should_stop()) break;

        if(runner->default_delay > 0) {
            if(!ducky_delay(runner->default_delay)) break;
        }
    }

    // Release any held keys
    ble_profile_hid_kb_release_all(runner->profile);

    if(runner->state == DuckyRunnerStateRunning && !runner->user_stopped) {
        runner->state = DuckyRunnerStateDone;
        if(runner->callback) runner->callback(runner->callback_context);
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    FURI_LOG_I(TAG, "Worker done");
    return 0;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

DuckyRunner* ducky_runner_alloc(void) {
    DuckyRunner* runner = malloc(sizeof(DuckyRunner));
    memset(runner, 0, sizeof(DuckyRunner));
    runner->state = DuckyRunnerStateIdle;
    return runner;
}

void ducky_runner_free(DuckyRunner* runner) {
    furi_assert(runner);
    ducky_runner_stop(runner); // ensures thread is joined before freeing
    free(runner);
}

bool ducky_runner_start(DuckyRunner* runner, FuriHalBleProfileBase* profile, const char* path) {
    furi_assert(runner);
    furi_assert(profile);
    furi_assert(path);

    if(runner->state == DuckyRunnerStateRunning) return false;

    runner->profile      = profile;
    runner->user_stopped = false;
    strlcpy(runner->path, path, sizeof(runner->path));
    runner->state    = DuckyRunnerStateIdle;
    runner->error[0] = '\0';

    if(runner->thread) {
        furi_thread_join(runner->thread);
        furi_thread_free(runner->thread);
    }

    runner->thread = furi_thread_alloc_ex("DuckyRunner", 2048, ducky_worker, runner);
    furi_thread_start(runner->thread);
    return true;
}

void ducky_runner_stop(DuckyRunner* runner) {
    furi_assert(runner);
    if(runner->thread == NULL) return;
    // Set user_stopped BEFORE the stop flag so the worker sees it before it tries to fire
    // the Done callback.  This prevents a spurious Done event from reaching the scene
    // manager after the run scene has already been popped.
    runner->user_stopped = true;
    furi_thread_flags_set(furi_thread_get_id(runner->thread), DUCKY_EVT_STOP);
    furi_thread_join(runner->thread);
    furi_thread_free(runner->thread);
    runner->thread = NULL;
}

DuckyRunnerState ducky_runner_get_state(const DuckyRunner* runner) {
    furi_assert(runner);
    return runner->state;
}

const char* ducky_runner_get_error(const DuckyRunner* runner) {
    furi_assert(runner);
    return runner->error;
}

void ducky_runner_set_callback(DuckyRunner* runner, DuckyRunnerCallback cb, void* context) {
    furi_assert(runner);
    runner->callback         = cb;
    runner->callback_context = context;
}
