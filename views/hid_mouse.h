#pragma once

#include <gui/view.h>
#include <input/input.h>

#define MOUSE_MOVE_SHORT 5
#define MOUSE_MOVE_LONG  20

typedef struct Hid Hid;
typedef struct HidMouse HidMouse;

// Standalone d-pad handler for "Mouse"-style cursor movement:
//   - InputTypePress: emit one MOUSE_MOVE_SHORT, set the matching *_pressed
//     flag, reset *acceleration to 1.
//   - InputTypeRepeat: emit MOUSE_MOVE_LONG repeatedly using the current
//     *acceleration; ramp *acceleration by +1 per repeat, capped at 20.
//   - InputTypeRelease: clear the matching *_pressed flag, reset *acceleration.
// Caller owns the view-model lock and passes pointers to the relevant model
// fields. Used both by the standalone Mouse view and by iOS Phone's Slow mode
// so the two stay behaviorally identical.
void hid_mouse_dpad_process(
    Hid*        hid,
    InputEvent* event,
    uint8_t*    acceleration,
    bool*       up_pressed,
    bool*       down_pressed,
    bool*       left_pressed,
    bool*       right_pressed);

HidMouse* hid_mouse_alloc(Hid* bt_hid);

void hid_mouse_free(HidMouse* hid_mouse);

View* hid_mouse_get_view(HidMouse* hid_mouse);

void hid_mouse_set_connected_status(HidMouse* hid_mouse, bool connected);

// Configure the short-press Back action. When enabled, a short Back press switches
// to view_id (e.g. returning to the Media remote) instead of sending a right-click.
// Default (disabled) keeps the right-click behavior. Long-press Back always exits.
void hid_mouse_set_back_to_view(HidMouse* hid_mouse, bool enabled, uint32_t view_id);
