#include "hid_custom_remote.h"
#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>
#include <furi.h>
#include <string.h>

// Short-label length: 8 chars + NUL.  At ~6 px/char this fits in ~50 px per column.
#define HID_CR_LABEL_LEN 9

// Single definition of the labels table (declared extern in the header).
const char* const custom_remote_input_labels[CustomRemoteInputCount] = {
    "Tap Up",   "Tap Down", "Tap Left", "Tap Right",
    "Hold Up",  "Hold Down","Hold Left","Hold Right",
    "Tap OK",   "Hold OK",  "Tap Back",
};

typedef struct {
    char remote_name[BT_REMOTES_CUSTOM_REMOTE_NAME_LEN];
    // Pre-computed display stems for each input slot
    char short_labels[CustomRemoteInputCount][HID_CR_LABEL_LEN];
} HidCustomRemoteModel;

struct HidCustomRemote {
    View*                   view;
    HidCustomRemoteCallback callback;
    void*                   callback_context;
};

// ---------------------------------------------------------------------------
// Shared stem extractor — declared in hid_custom_remote.h.
// Extracts the filename stem from a full path, truncated to out_size-1 chars.
// Empty/NULL path → "-".
// ---------------------------------------------------------------------------

void hid_custom_remote_stem(const char* path, char* out, size_t out_size) {
    if(!path || path[0] == '\0') {
        strlcpy(out, "-", out_size);
        return;
    }
    const char* base = strrchr(path, '/');
    base = base ? base + 1 : path;
    strlcpy(out, base, out_size); // auto-truncates to out_size-1
    char* dot = strrchr(out, '.');
    if(dot) *dot = '\0';
    if(out[0] == '\0') strlcpy(out, "-", out_size);
}

// ---------------------------------------------------------------------------
// Draw callback
// Layout (128×64):
//   y=9  : remote name (FontPrimary, centred)
//   y=11 : thin separator
//   Left col  (x=0)  : tap inputs   — prefix + stem
//   Right col (x=65) : hold inputs  — prefix + stem
//   Rows y=19,27,35,43,51,59
// ---------------------------------------------------------------------------

static void hid_custom_remote_draw_cb(Canvas* canvas, void* model_ptr) {
    HidCustomRemoteModel* model = model_ptr;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    // Title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, model->remote_name);

    // Separator line
    canvas_draw_line(canvas, 0, 11, 127, 11);

    canvas_set_font(canvas, FontSecondary);

    // Row definitions: each row has a tap slot (left) and hold slot (right).
    // Row 6 (Back row) has no hold input — right side shows the exit hint.
    static const struct {
        const char* tap_prefix;
        uint8_t     tap_slot;
        const char* hold_prefix;
        int         hold_slot; // -1 = show hint
    } rows[] = {
        {"^",  CustomRemoteInputTapUp,    "H^",  CustomRemoteInputHoldUp},
        {"v",  CustomRemoteInputTapDown,  "Hv",  CustomRemoteInputHoldDown},
        {"<",  CustomRemoteInputTapLeft,  "H<",  CustomRemoteInputHoldLeft},
        {">",  CustomRemoteInputTapRight, "H>",  CustomRemoteInputHoldRight},
        {"OK", CustomRemoteInputTapOk,    "HOK", CustomRemoteInputHoldOk},
        {"Bk", CustomRemoteInputTapBack,  NULL,  -1}, // right col = exit hint
    };

    for(int r = 0; r < 6; r++) {
        int y = 19 + r * 8;

        // Left column: tap prefix + stem
        char buf[32];
        snprintf(buf, sizeof(buf), "%s %s",
                 rows[r].tap_prefix,
                 model->short_labels[rows[r].tap_slot]);
        canvas_draw_str(canvas, 0, y, buf);

        // Right column
        if(rows[r].hold_slot >= 0) {
            snprintf(buf, sizeof(buf), "%s %s",
                     rows[r].hold_prefix,
                     model->short_labels[rows[r].hold_slot]);
            canvas_draw_str(canvas, 65, y, buf);
        } else {
            // Back row right side: exit hint
            canvas_draw_str(canvas, 65, y, "HBk=exit");
        }
    }
}

// ---------------------------------------------------------------------------
// Input callback
// Short presses  → tap slots (all except Back: TapBack on Short-Back)
// Long presses   → hold slots (except Long-Back which exits)
// Release/Repeat → consumed silently
// ---------------------------------------------------------------------------

static bool hid_custom_remote_input_cb(InputEvent* event, void* context) {
    furi_assert(context);
    HidCustomRemote* cr = context;

    // Ignore Press, Release, Repeat — only handle Short and Long
    if(event->type != InputTypeShort && event->type != InputTypeLong) {
        return true; // consume silently
    }

    // Long Back → exit the view (return false so scene manager fires Back)
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        return false;
    }

    CustomRemoteInputSlot slot;
    bool valid = true;

    if(event->type == InputTypeShort) {
        switch(event->key) {
        case InputKeyUp:    slot = CustomRemoteInputTapUp;    break;
        case InputKeyDown:  slot = CustomRemoteInputTapDown;  break;
        case InputKeyLeft:  slot = CustomRemoteInputTapLeft;  break;
        case InputKeyRight: slot = CustomRemoteInputTapRight; break;
        case InputKeyOk:    slot = CustomRemoteInputTapOk;    break;
        case InputKeyBack:  slot = CustomRemoteInputTapBack;  break;
        default:            valid = false;                    break;
        }
    } else { // InputTypeLong
        switch(event->key) {
        case InputKeyUp:    slot = CustomRemoteInputHoldUp;    break;
        case InputKeyDown:  slot = CustomRemoteInputHoldDown;  break;
        case InputKeyLeft:  slot = CustomRemoteInputHoldLeft;  break;
        case InputKeyRight: slot = CustomRemoteInputHoldRight; break;
        case InputKeyOk:    slot = CustomRemoteInputHoldOk;    break;
        default:            valid = false;                     break;
        }
    }

    if(valid && cr->callback) {
        bool handled = cr->callback(cr->callback_context, slot);
        // For Short Back specifically: if the callback returned false (slot unassigned),
        // propagate the event so the scene manager pops the scene.
        if(!handled && slot == CustomRemoteInputTapBack) {
            return false;
        }
    }

    return true; // consume all other inputs regardless of assignment
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

HidCustomRemote* hid_custom_remote_alloc(void) {
    HidCustomRemote* cr = malloc(sizeof(HidCustomRemote));
    cr->view = view_alloc();
    view_set_context(cr->view, cr);
    view_allocate_model(cr->view, ViewModelTypeLocking, sizeof(HidCustomRemoteModel));
    view_set_draw_callback(cr->view, hid_custom_remote_draw_cb);
    view_set_input_callback(cr->view, hid_custom_remote_input_cb);

    with_view_model(
        cr->view,
        HidCustomRemoteModel * model,
        {
            model->remote_name[0] = '\0';
            for(uint8_t i = 0; i < CustomRemoteInputCount; i++) {
                strlcpy(model->short_labels[i], "-", HID_CR_LABEL_LEN);
            }
        },
        false);

    cr->callback         = NULL;
    cr->callback_context = NULL;
    return cr;
}

void hid_custom_remote_free(HidCustomRemote* cr) {
    furi_assert(cr);
    view_free(cr->view);
    free(cr);
}

View* hid_custom_remote_get_view(HidCustomRemote* cr) {
    furi_assert(cr);
    return cr->view;
}

void hid_custom_remote_set_remote(HidCustomRemote* cr, const CustomRemoteDef* def) {
    furi_assert(cr);
    furi_assert(def);
    with_view_model(
        cr->view,
        HidCustomRemoteModel * model,
        {
            strlcpy(model->remote_name, def->name, sizeof(model->remote_name));
            for(uint8_t i = 0; i < CustomRemoteInputCount; i++) {
                hid_custom_remote_stem(def->scripts[i], model->short_labels[i], HID_CR_LABEL_LEN);
            }
        },
        true);
}

void hid_custom_remote_set_callback(
    HidCustomRemote*        cr,
    HidCustomRemoteCallback cb,
    void*                   context) {
    furi_assert(cr);
    cr->callback         = cb;
    cr->callback_context = context;
}
