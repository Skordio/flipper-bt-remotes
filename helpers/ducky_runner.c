#include "ducky_runner.h"
#include <furi_hal.h>
#include <furi_hal_usb_hid.h>
#include <storage/storage.h>
#include <ble_profile/extra_profiles/hid_profile.h>

#define TAG "DuckyRunner"

// Worker thread stop flag
#define DUCKY_EVT_STOP (1u << 0)

// Maximum characters per script line
#define DUCKY_LINE_MAX 512

// ---------------------------------------------------------------------------
// Key lookup tables
// ---------------------------------------------------------------------------

typedef struct {
    const char* name;
    uint16_t    keycode;
} DuckyKeyEntry;

// Modifier keys — all produce a bitmask in the high byte of the HID keycode word.
static const DuckyKeyEntry ducky_modifiers[] = {
    {"CTRL",    KEY_MOD_LEFT_CTRL},
    {"CONTROL", KEY_MOD_LEFT_CTRL},
    {"SHIFT",   KEY_MOD_LEFT_SHIFT},
    {"ALT",     KEY_MOD_LEFT_ALT},
    {"GUI",     KEY_MOD_LEFT_GUI},
    {"WINDOWS", KEY_MOD_LEFT_GUI},
    {"COMMAND", KEY_MOD_LEFT_GUI}, // Mac alias
};

// Named keyboard keys.
static const DuckyKeyEntry ducky_named_keys[] = {
    {"ENTER",        HID_KEYBOARD_RETURN},
    {"RETURN",       HID_KEYBOARD_RETURN},
    {"TAB",          HID_KEYBOARD_TAB},
    {"SPACE",        HID_KEYBOARD_SPACEBAR},
    {"BACKSPACE",    HID_KEYBOARD_DELETE},
    {"DELETE",       HID_KEYBOARD_DELETE_FORWARD},
    {"ESC",          HID_KEYBOARD_ESCAPE},
    {"ESCAPE",       HID_KEYBOARD_ESCAPE},
    {"UP",           HID_KEYBOARD_UP_ARROW},
    {"UPARROW",      HID_KEYBOARD_UP_ARROW},
    {"DOWN",         HID_KEYBOARD_DOWN_ARROW},
    {"DOWNARROW",    HID_KEYBOARD_DOWN_ARROW},
    {"LEFT",         HID_KEYBOARD_LEFT_ARROW},
    {"LEFTARROW",    HID_KEYBOARD_LEFT_ARROW},
    {"RIGHT",        HID_KEYBOARD_RIGHT_ARROW},
    {"RIGHTARROW",   HID_KEYBOARD_RIGHT_ARROW},
    {"HOME",         HID_KEYBOARD_HOME},
    {"END",          HID_KEYBOARD_END},
    {"PAGEUP",       HID_KEYBOARD_PAGE_UP},
    {"PAGEDOWN",     HID_KEYBOARD_PAGE_DOWN},
    {"INSERT",       HID_KEYBOARD_INSERT},
    {"PRINTSCREEN",  HID_KEYBOARD_PRINT_SCREEN},
    {"CAPSLOCK",     HID_KEYBOARD_CAPS_LOCK},
    {"NUMLOCK",      HID_KEYPAD_NUMLOCK},
    {"SCROLLLOCK",   HID_KEYBOARD_SCROLL_LOCK},
    {"PAUSE",        HID_KEYBOARD_PAUSE},
    {"BREAK",        HID_KEYBOARD_PAUSE},
    {"MENU",         HID_KEYBOARD_APPLICATION},
    {"APP",          HID_KEYBOARD_APPLICATION},
    {"F1",           HID_KEYBOARD_F1},
    {"F2",           HID_KEYBOARD_F2},
    {"F3",           HID_KEYBOARD_F3},
    {"F4",           HID_KEYBOARD_F4},
    {"F5",           HID_KEYBOARD_F5},
    {"F6",           HID_KEYBOARD_F6},
    {"F7",           HID_KEYBOARD_F7},
    {"F8",           HID_KEYBOARD_F8},
    {"F9",           HID_KEYBOARD_F9},
    {"F10",          HID_KEYBOARD_F10},
    {"F11",          HID_KEYBOARD_F11},
    {"F12",          HID_KEYBOARD_F12},
    {"F13",          HID_KEYBOARD_F13},
    {"F14",          HID_KEYBOARD_F14},
    {"F15",          HID_KEYBOARD_F15},
    {"F16",          HID_KEYBOARD_F16},
    {"F17",          HID_KEYBOARD_F17},
    {"F18",          HID_KEYBOARD_F18},
    {"F19",          HID_KEYBOARD_F19},
    {"F20",          HID_KEYBOARD_F20},
    {"F21",          HID_KEYBOARD_F21},
    {"F22",          HID_KEYBOARD_F22},
    {"F23",          HID_KEYBOARD_F23},
    {"F24",          HID_KEYBOARD_F24},
};

// Consumer / media control keys used with the MEDIA command.
typedef struct {
    const char* name;
    uint16_t    consumer_code;
} DuckyMediaEntry;

static const DuckyMediaEntry ducky_media_keys[] = {
    {"POWER",       HID_CONSUMER_POWER},
    {"REBOOT",      HID_CONSUMER_RESET},
    {"SLEEP",       HID_CONSUMER_SLEEP},
    {"LOGOFF",      HID_CONSUMER_AL_LOGOFF},
    {"EXIT",        HID_CONSUMER_AC_EXIT},
    {"HOME",        HID_CONSUMER_AC_HOME},
    {"BACK",        HID_CONSUMER_AC_BACK},
    {"FORWARD",     HID_CONSUMER_AC_FORWARD},
    {"REFRESH",     HID_CONSUMER_AC_REFRESH},
    {"SNAPSHOT",    HID_CONSUMER_SNAPSHOT},
    {"PLAY",        HID_CONSUMER_PLAY},
    {"PAUSE",       HID_CONSUMER_PAUSE},
    {"PLAY_PAUSE",  HID_CONSUMER_PLAY_PAUSE},
    {"NEXT_TRACK",  HID_CONSUMER_SCAN_NEXT_TRACK},
    {"PREV_TRACK",  HID_CONSUMER_SCAN_PREVIOUS_TRACK},
    {"STOP",        HID_CONSUMER_STOP},
    {"EJECT",       HID_CONSUMER_EJECT},
    {"MUTE",        HID_CONSUMER_MUTE},
    {"VOLUME_UP",   HID_CONSUMER_VOLUME_INCREMENT},
    {"VOLUME_DOWN", HID_CONSUMER_VOLUME_DECREMENT},
    {"FN",          HID_CONSUMER_FN_GLOBE},
    {"BRIGHT_UP",   HID_CONSUMER_BRIGHTNESS_INCREMENT},
    {"BRIGHT_DOWN", HID_CONSUMER_BRIGHTNESS_DECREMENT},
};

// Mouse button keys — usable standalone (press+release) or with HOLD/RELEASE.
typedef struct {
    const char* name;
    uint8_t     button;
} DuckyMouseEntry;

static const DuckyMouseEntry ducky_mouse_keys[] = {
    {"LEFTCLICK",    HID_MOUSE_BTN_LEFT},
    {"LEFT_CLICK",   HID_MOUSE_BTN_LEFT},
    {"RIGHTCLICK",   HID_MOUSE_BTN_RIGHT},
    {"RIGHT_CLICK",  HID_MOUSE_BTN_RIGHT},
    {"MIDDLECLICK",  HID_MOUSE_BTN_WHEEL},
    {"MIDDLE_CLICK", HID_MOUSE_BTN_WHEEL},
    {"WHEELCLICK",   HID_MOUSE_BTN_WHEEL},
    {"WHEEL_CLICK",  HID_MOUSE_BTN_WHEEL},
};

// ---------------------------------------------------------------------------
// Struct
// ---------------------------------------------------------------------------

struct DuckyRunner {
    FuriThread*            thread;
    FuriHalBleProfileBase* profile;
    char                   path[256];
    char                   error[64];
    volatile DuckyRunnerState state;
    volatile bool          user_stopped; // guards against spurious Done event on Back
    uint32_t               default_delay;        // ms appended after each command
    uint32_t               string_delay;         // one-shot per-char delay (resets after STRING)
    uint32_t               default_string_delay; // persistent per-char delay (DEFAULT_STRING_DELAY)
    uint8_t                key_hold_nb;          // number of currently held keys
    DuckyRunnerCallback    callback;
    void*                  callback_context;
};

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

// Interruptible delay: returns false if the stop flag fired, true on normal expiry.
static bool ducky_delay(uint32_t ms) {
    while(ms > 0) {
        uint32_t chunk = ms > 100 ? 100 : ms;
        uint32_t flags = furi_thread_flags_wait(DUCKY_EVT_STOP, FuriFlagWaitAny, chunk);
        if((flags & FuriFlagError) == 0) return false; // stop flag received
        ms -= chunk;
    }
    return true;
}

// Non-blocking stop check; clears the flag if set.
static bool ducky_should_stop(void) {
    uint32_t flags = furi_thread_flags_get();
    if(flags & DUCKY_EVT_STOP) {
        furi_thread_flags_clear(DUCKY_EVT_STOP);
        return true;
    }
    return false;
}

// Read one text line (strips \r\n, NUL-terminates). Returns false at EOF.
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

// Returns true if *p starts with name followed by a valid delimiter (space / - / end).
// Advances *p past the matched word.
static bool ducky_match_word(const char** p, const char* name) {
    size_t len = strlen(name);
    if(strncmp(*p, name, len) != 0) return false;
    char after = (*p)[len];
    if(after != ' ' && after != '-' && after != '\0' && after != '\r' && after != '\n')
        return false;
    *p += len;
    return true;
}

// Resolve a single token (e.g. "CTRL", "F1", "a") to its HID keyboard keycode.
// Modifier keys: high byte set.  Regular keys: low byte only.
// Returns 0 if unrecognised.
static uint16_t ducky_lookup_keyboard_key(const char* token) {
    for(size_t i = 0; i < COUNT_OF(ducky_modifiers); i++) {
        if(strcmp(token, ducky_modifiers[i].name) == 0)
            return ducky_modifiers[i].keycode;
    }
    for(size_t i = 0; i < COUNT_OF(ducky_named_keys); i++) {
        if(strcmp(token, ducky_named_keys[i].name) == 0)
            return ducky_named_keys[i].keycode;
    }
    // Single ASCII character — strip implicit shift so HOLD A = bare 'a' scancode.
    if(token[0] != '\0' && token[1] == '\0' && (uint8_t)token[0] < 128)
        return HID_ASCII_TO_KEY((uint8_t)token[0]) & 0x00FF;
    return 0;
}

// Resolve a mouse button name to its bitmask. Returns 0 if unrecognised.
static uint8_t ducky_lookup_mouse_key(const char* token) {
    for(size_t i = 0; i < COUNT_OF(ducky_mouse_keys); i++) {
        if(strcmp(token, ducky_mouse_keys[i].name) == 0)
            return ducky_mouse_keys[i].button;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// Execution helpers
// ---------------------------------------------------------------------------

static void ducky_type_string(DuckyRunner* runner, const char* text) {
    // Effective per-char delay: one-shot override takes precedence over persistent default.
    uint32_t delay = runner->string_delay > 0
                         ? runner->string_delay
                         : runner->default_string_delay;

    while(*text && *text != '\r' && *text != '\n') {
        uint16_t keycode = HID_ASCII_TO_KEY((uint8_t)*text);
        if(keycode != HID_KEYBOARD_NONE) {
            ble_profile_hid_kb_press(runner->profile, keycode);
            ble_profile_hid_kb_release(runner->profile, keycode);
        }
        text++;
        if(delay > 0) {
            if(!ducky_delay(delay)) return;
        }
    }
    runner->string_delay = 0; // one-shot: clear after each STRING/STRINGLN
}

// Parse and send a modifier+key combo (e.g. "CTRL ALT DELETE", "GUI r", "F4").
static void ducky_press_combo(DuckyRunner* runner, const char* line) {
    uint16_t    combo = 0;
    const char* p     = line;

    while(*p && *p != '\r' && *p != '\n') {
        while(*p == ' ' || *p == '-') p++;
        if(!*p || *p == '\r' || *p == '\n') break;

        // Try each modifier (may chain multiple)
        bool found_mod = false;
        for(size_t i = 0; i < COUNT_OF(ducky_modifiers); i++) {
            if(ducky_match_word(&p, ducky_modifiers[i].name)) {
                combo    |= ducky_modifiers[i].keycode;
                found_mod = true;
                break;
            }
        }
        if(found_mod) continue;

        // Try a named key — once matched, parsing stops
        bool found_key = false;
        for(size_t i = 0; i < COUNT_OF(ducky_named_keys); i++) {
            if(ducky_match_word(&p, ducky_named_keys[i].name)) {
                combo    |= ducky_named_keys[i].keycode;
                found_key = true;
                break;
            }
        }
        if(found_key) break;

        // Single ASCII character — strip shift so "CTRL a" sends Ctrl+a not Ctrl+Shift+A.
        if((uint8_t)*p < 128) {
            uint16_t k = HID_ASCII_TO_KEY((uint8_t)*p) & 0x00FF;
            if(k != HID_KEYBOARD_NONE) combo |= k;
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

// Shared logic for HOLD and RELEASE.
static void ducky_do_hold_release(DuckyRunner* runner, const char* token, bool hold) {
    // Mouse key?
    uint8_t mbtn = ducky_lookup_mouse_key(token);
    if(mbtn != 0) {
        if(hold) {
            ble_profile_hid_mouse_press(runner->profile, mbtn);
            runner->key_hold_nb++;
        } else {
            ble_profile_hid_mouse_release(runner->profile, mbtn);
            if(runner->key_hold_nb > 0) runner->key_hold_nb--;
        }
        return;
    }
    // Keyboard key (modifier or named)?
    uint16_t key = ducky_lookup_keyboard_key(token);
    if(key != 0) {
        if(hold) {
            ble_profile_hid_kb_press(runner->profile, key);
            runner->key_hold_nb++;
        } else {
            ble_profile_hid_kb_release(runner->profile, key);
            if(runner->key_hold_nb > 0) runner->key_hold_nb--;
        }
        return;
    }
    // Unrecognised — silently ignore (matches official bad_kb behaviour)
}

// Execute one DuckyScript line. Returns false if execution should stop.
static bool ducky_exec_line(DuckyRunner* runner, const char* raw) {
    // Trim leading whitespace
    const char* line = raw;
    while(*line == ' ' || *line == '\t') line++;

    // Empty line / comment
    if(*line == '\0' || *line == '\r' || *line == '\n') return true;
    if(strncmp(line, "REM", 3) == 0 && (line[3] == ' ' || line[3] == '\0')) return true;
    if(strncmp(line, "//",  2) == 0) return true;

    // STRING / STRINGLN -------------------------------------------------------
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

    // DELAY -------------------------------------------------------------------
    if(strncmp(line, "DELAY ", 6) == 0)
        return ducky_delay((uint32_t)atoi(line + 6));

    // DEFAULTDELAY / DEFAULT_DELAY --------------------------------------------
    if(strncmp(line, "DEFAULTDELAY ", 13) == 0) {
        runner->default_delay = (uint32_t)atoi(line + 13);
        return true;
    }
    if(strncmp(line, "DEFAULT_DELAY ", 14) == 0) {
        runner->default_delay = (uint32_t)atoi(line + 14);
        return true;
    }

    // STRINGDELAY / STRING_DELAY (one-shot, resets after next STRING) ----------
    if(strncmp(line, "STRINGDELAY ", 12) == 0) {
        runner->string_delay = (uint32_t)atoi(line + 12);
        return true;
    }
    if(strncmp(line, "STRING_DELAY ", 13) == 0) {
        runner->string_delay = (uint32_t)atoi(line + 13);
        return true;
    }

    // DEFAULTSTRINGDELAY / DEFAULT_STRING_DELAY (persistent) ------------------
    if(strncmp(line, "DEFAULTSTRINGDELAY ", 19) == 0) {
        runner->default_string_delay = (uint32_t)atoi(line + 19);
        return true;
    }
    if(strncmp(line, "DEFAULT_STRING_DELAY ", 21) == 0) {
        runner->default_string_delay = (uint32_t)atoi(line + 21);
        return true;
    }

    // HOLD / RELEASE ----------------------------------------------------------
    if(strncmp(line, "HOLD ", 5) == 0) {
        ducky_do_hold_release(runner, line + 5, true);
        return true;
    }
    if(strncmp(line, "RELEASE ", 8) == 0) {
        ducky_do_hold_release(runner, line + 8, false);
        return true;
    }

    // GLOBE <key> — presses the Apple Globe/Fn consumer key together with a
    // keyboard key. Used for iPhone shortcuts (GLOBE h = Home, GLOBE l = Lock…).
    if(strncmp(line, "GLOBE ", 6) == 0) {
        uint16_t key = ducky_lookup_keyboard_key(line + 6);
        if(key != 0) {
            ble_profile_hid_consumer_key_press(runner->profile, HID_CONSUMER_FN_GLOBE);
            ble_profile_hid_kb_press(runner->profile, key);
            ble_profile_hid_kb_release(runner->profile, key);
            ble_profile_hid_consumer_key_release(runner->profile, HID_CONSUMER_FN_GLOBE);
        }
        return true;
    }

    // MEDIA <name> ------------------------------------------------------------
    if(strncmp(line, "MEDIA ", 6) == 0) {
        const char* name = line + 6;
        for(size_t i = 0; i < COUNT_OF(ducky_media_keys); i++) {
            if(strcmp(name, ducky_media_keys[i].name) == 0) {
                ble_profile_hid_consumer_key_press(
                    runner->profile, ducky_media_keys[i].consumer_code);
                ble_profile_hid_consumer_key_release(
                    runner->profile, ducky_media_keys[i].consumer_code);
                break;
            }
        }
        return true;
    }

    // MOUSEMOVE / MOUSE_MOVE <x> <y> ------------------------------------------
    // ble_profile_hid_mouse_move takes int8_t so we loop for large deltas.
    const char* mm_arg = NULL;
    if(strncmp(line, "MOUSEMOVE ", 10) == 0)       mm_arg = line + 10;
    else if(strncmp(line, "MOUSE_MOVE ", 11) == 0) mm_arg = line + 11;
    if(mm_arg) {
        int32_t     dx    = (int32_t)atoi(mm_arg);
        const char* space = strchr(mm_arg, ' ');
        int32_t     dy    = space ? (int32_t)atoi(space + 1) : 0;
        while(dx != 0 || dy != 0) {
            int8_t sx = (int8_t)(dx > 127 ? 127 : dx < -128 ? -128 : dx);
            int8_t sy = (int8_t)(dy > 127 ? 127 : dy < -128 ? -128 : dy);
            ble_profile_hid_mouse_move(runner->profile, sx, sy);
            dx -= sx;
            dy -= sy;
            if((dx != 0 || dy != 0) && !ducky_delay(10)) return false;
        }
        return true;
    }

    // MOUSESCROLL / MOUSE_SCROLL <n> ------------------------------------------
    const char* ms_arg = NULL;
    if(strncmp(line, "MOUSESCROLL ", 12) == 0)       ms_arg = line + 12;
    else if(strncmp(line, "MOUSE_SCROLL ", 13) == 0) ms_arg = line + 13;
    if(ms_arg) {
        ble_profile_hid_mouse_scroll(runner->profile, (int8_t)atoi(ms_arg));
        return true;
    }

    // Mouse click keys (standalone press + immediate release) -----------------
    for(size_t i = 0; i < COUNT_OF(ducky_mouse_keys); i++) {
        if(strcmp(line, ducky_mouse_keys[i].name) == 0) {
            ble_profile_hid_mouse_press(runner->profile, ducky_mouse_keys[i].button);
            ble_profile_hid_mouse_release(runner->profile, ducky_mouse_keys[i].button);
            return true;
        }
    }

    // Commands handled in the worker loop or intentionally skipped -------------
    if(strncmp(line, "REPEAT",          6) == 0) return true; // handled above the exec call
    if(strncmp(line, "WAITFORBUTTON",  13) == 0) return true;
    if(strncmp(line, "WAIT_FOR_BUTTON",15) == 0) return true;
    if(strncmp(line, "ID ",             3) == 0) return true;
    if(strncmp(line, "BT_ID ",          6) == 0) return true;
    if(strncmp(line, "BLE_ID ",         7) == 0) return true;
    if(strncmp(line, "ATTACKMODE",     10) == 0) return true;
    if(strncmp(line, "LED",             3) == 0) return true;

    // Everything else: treat as a modifier+key combo (e.g. "CTRL ALT DELETE") -
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

    runner->default_delay        = 0;
    runner->string_delay         = 0;
    runner->default_string_delay = 0;
    runner->key_hold_nb          = 0;
    runner->state                = DuckyRunnerStateRunning;
    if(runner->callback) runner->callback(runner->callback_context);

    char line[DUCKY_LINE_MAX];
    char prev_line[DUCKY_LINE_MAX];
    prev_line[0] = '\0';
    bool keep_going = true;

    while(keep_going && ducky_read_line(file, line, sizeof(line))) {
        if(ducky_should_stop()) break;

        // Trim for classification only (exec functions do their own trimming)
        const char* trimmed = line;
        while(*trimmed == ' ' || *trimmed == '\t') trimmed++;

        // REPEAT N — re-execute the previous substantive line N times.
        if(strncmp(trimmed, "REPEAT ", 7) == 0 && prev_line[0] != '\0') {
            uint32_t n = (uint32_t)atoi(trimmed + 7);
            for(uint32_t r = 0; r < n && keep_going; r++) {
                if(ducky_should_stop()) {
                    keep_going = false;
                    break;
                }
                keep_going = ducky_exec_line(runner, prev_line);
                if(!keep_going || runner->state == DuckyRunnerStateError) break;
                if(runner->default_delay > 0 && r < n - 1) {
                    if(!ducky_delay(runner->default_delay)) {
                        keep_going = false;
                        break;
                    }
                }
            }
            // REPEAT itself does not update prev_line
            continue;
        }

        // Track prev_line — update only for non-empty, non-comment, non-REPEAT lines.
        bool is_empty = (*trimmed == '\0' || *trimmed == '\r' || *trimmed == '\n');
        bool is_comment = !is_empty &&
            ((strncmp(trimmed, "REM", 3) == 0 && (trimmed[3] == ' ' || trimmed[3] == '\0')) ||
              strncmp(trimmed, "//", 2) == 0);
        if(!is_empty && !is_comment) {
            strlcpy(prev_line, line, sizeof(prev_line));
        }

        keep_going = ducky_exec_line(runner, line);

        if(!keep_going || runner->state == DuckyRunnerStateError) break;
        if(ducky_should_stop()) break;

        if(runner->default_delay > 0) {
            if(!ducky_delay(runner->default_delay)) break;
        }
    }

    // Release everything that might be held
    ble_profile_hid_kb_release_all(runner->profile);
    ble_profile_hid_mouse_release_all(runner->profile);
    ble_profile_hid_consumer_key_release_all(runner->profile);

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
    ducky_runner_stop(runner);
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
    // Set user_stopped BEFORE the stop flag so the worker sees it before
    // it can fire the Done callback, preventing a spurious event.
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
