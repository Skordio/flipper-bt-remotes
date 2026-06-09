#include "hid_tiktok.h"
#include "../hid.h"
#include <gui/elements.h>

#include "hid_icons.h"

#define TAG "HidTikTok"

struct HidTikTok {
    View* view;
    Hid* hid;
};

typedef struct {
    bool left_pressed;
    bool up_pressed;
    bool right_pressed;
    bool down_pressed;
    bool ok_pressed;
    bool connected;
    bool is_cursor_set;
    bool back_mouse_pressed;
    bool gesture_mode; // Up/Down use swipe gestures instead of the scroll wheel
} HidTikTokModel;

static void hid_tiktok_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    HidTikTokModel* model = context;

    // Header
#ifdef HID_TRANSPORT_BLE
    if(model->connected) {
        canvas_draw_icon(canvas, 0, 0, &I_Ble_connected_15x15);
    } else {
        canvas_draw_icon(canvas, 0, 0, &I_Ble_disconnected_15x15);
    }
#endif

    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 17, 3, AlignLeft, AlignTop, "TikTok /");
    elements_multiline_text_aligned(canvas, 3, 18, AlignLeft, AlignTop, "YT Shorts");
    canvas_set_font(canvas, FontSecondary);

    // Gesture-mode indicator (Up/Down emulate finger swipes)
    if(model->gesture_mode) {
        elements_multiline_text_aligned(canvas, 3, 33, AlignLeft, AlignTop, "Swipe mode");
    }

    // Keypad circles
    canvas_draw_icon(canvas, 58, 3, &I_OutCircles_70x51);

    // Pause
    if(model->back_mouse_pressed) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 107, 33, &I_Pressed_Button_19x19);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 113, 37, &I_Pause_icon_9x9);
    canvas_set_color(canvas, ColorBlack);

    // Up
    if(model->up_pressed) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 68, 6, &I_S_UP_31x15);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 80, 8, &I_Arr_up_7x9);
    canvas_set_color(canvas, ColorBlack);

    // Down
    if(model->down_pressed) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 68, 36, &I_S_DOWN_31x15);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 80, 40, &I_Arr_dwn_7x9);
    canvas_set_color(canvas, ColorBlack);

    // Left
    if(model->left_pressed) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 61, 13, &I_S_LEFT_15x31);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 64, 25, &I_Voldwn_6x6);
    canvas_set_color(canvas, ColorBlack);

    // Right
    if(model->right_pressed) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 91, 13, &I_S_RIGHT_15x31);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 95, 25, &I_Volup_8x6);
    canvas_set_color(canvas, ColorBlack);

    // Ok
    if(model->ok_pressed) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 74, 19, &I_Pressed_Button_19x19);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 78, 25, &I_Like_def_11x9);
    canvas_set_color(canvas, ColorBlack);

    // Exit
    canvas_draw_icon(canvas, 0, 54, &I_Pin_back_arrow_10x8);
    canvas_set_font(canvas, FontSecondary);
    elements_multiline_text_aligned(canvas, 13, 62, AlignLeft, AlignBottom, "Hold to exit");
}

static void hid_tiktok_reset_cursor(HidTikTok* hid_tiktok) {
    // Set cursor to the phone's left up corner
    // Delays to guarantee one packet per connection interval
    for(size_t i = 0; i < 8; i++) {
        hid_hal_mouse_move(hid_tiktok->hid, -127, -127);
        furi_delay_ms(50);
    }
    // Move cursor from the corner
    // Actions split for some mobiles to properly process mouse movements
    hid_hal_mouse_move(hid_tiktok->hid, 10, 60);
    furi_delay_ms(3);
    hid_hal_mouse_move(hid_tiktok->hid, 0, 60);
    furi_delay_ms(50);
}

// Emit a single-axis move of total px (signed) split into int8_t-safe chunks of
// at most |chunk| px, pacing each packet by delay_ms (one per connection
// interval). Exactly one of dx/dy is non-zero per call; pass the total on that
// axis as `total` and the desired sign is carried by `total`.
static void hid_tiktok_move_axis(
    HidTikTok* hid_tiktok,
    bool       horizontal,
    int        total,
    int        chunk,
    uint32_t   delay_ms) {
    while(total != 0) {
        int step = total;
        if(step > chunk) step = chunk;
        if(step < -chunk) step = -chunk;
        if(horizontal) {
            hid_hal_mouse_move(hid_tiktok->hid, (int8_t)step, 0);
        } else {
            hid_hal_mouse_move(hid_tiktok->hid, 0, (int8_t)step);
        }
        furi_delay_ms(delay_ms);
        total -= step;
    }
}

// Emulate a finger swipe via a mouse click-drag.
//   dir = -1 : drag up   (content scrolls up  -> NEXT video)
//   dir = +1 : drag down (content scrolls down -> PREVIOUS video)
// Re-anchors the cursor to a known corner each call (the host clamps movement at
// the edge), then moves INWARD off that edge to a start point with room to drag,
// then press -> stepped drag -> release. Delays pace one packet per BLE
// connection interval, mirroring hid_tiktok_reset_cursor.
// The three distances are per-profile settings (defaults match the original
// hardcoded 70 / 180 / 350):
//   tiktok_gesture_inset  — horizontal inset off the side edge (may exceed int8)
//   tiktok_gesture_margin — vertical travel off the top/bottom edge before press
//   tiktok_gesture_swipe  — drag distance while the button is held
static void hid_tiktok_gesture_swipe(HidTikTok* hid_tiktok, int dir) {
    const int inset  = hid_tiktok->hid->tiktok_gesture_inset;
    const int margin = hid_tiktok->hid->tiktok_gesture_margin;
    const int swipe  = hid_tiktok->hid->tiktok_gesture_swipe;

    // Anchor to the corner the drag starts from: an up-drag starts low (bottom),
    // a down-drag starts high (top). Horizontal anchor is the left edge.
    const int8_t v_anchor = (int8_t)(-dir * 127);
    for(size_t i = 0; i < 8; i++) {
        hid_hal_mouse_move(hid_tiktok->hid, -127, v_anchor);
        furi_delay_ms(40);
    }
    // Move inward to the start point: toward horizontal center, and well off the
    // anchored edge so the swipe doesn't begin in a screen-edge gesture zone.
    // Both the inset and the vertical margin are emitted in <=90px chunks (the
    // inset can exceed the int8_t per-move cap).
    hid_tiktok_move_axis(hid_tiktok, true, inset, 90, 20);
    furi_delay_ms(20);
    hid_tiktok_move_axis(hid_tiktok, false, dir * margin, 90, 20);

    // Touch down, drag in the swipe direction across the screen, touch up.
    // The drag is emitted in 35px steps (~10 steps at the 350 default) for a
    // smooth, evenly paced gesture.
    hid_hal_mouse_press(hid_tiktok->hid, HID_MOUSE_BTN_LEFT);
    furi_delay_ms(40);
    hid_tiktok_move_axis(hid_tiktok, false, dir * swipe, 35, 15);
    furi_delay_ms(40);
    hid_hal_mouse_release(hid_tiktok->hid, HID_MOUSE_BTN_LEFT);
}

static void
    hid_tiktok_process_press(HidTikTok* hid_tiktok, HidTikTokModel* model, InputEvent* event) {
    if(event->key == InputKeyUp) {
        model->up_pressed = true;
    } else if(event->key == InputKeyDown) {
        model->down_pressed = true;
    } else if(event->key == InputKeyLeft) {
        model->left_pressed = true;
        hid_hal_consumer_key_press(hid_tiktok->hid, HID_CONSUMER_VOLUME_DECREMENT);
    } else if(event->key == InputKeyRight) {
        model->right_pressed = true;
        hid_hal_consumer_key_press(hid_tiktok->hid, HID_CONSUMER_VOLUME_INCREMENT);
    } else if(event->key == InputKeyOk) {
        model->ok_pressed = true;
    } else if(event->key == InputKeyBack) {
        model->back_mouse_pressed = true;
    }
}

static void
    hid_tiktok_process_release(HidTikTok* hid_tiktok, HidTikTokModel* model, InputEvent* event) {
    if(event->key == InputKeyUp) {
        model->up_pressed = false;
    } else if(event->key == InputKeyDown) {
        model->down_pressed = false;
    } else if(event->key == InputKeyLeft) {
        model->left_pressed = false;
        hid_hal_consumer_key_release(hid_tiktok->hid, HID_CONSUMER_VOLUME_DECREMENT);
    } else if(event->key == InputKeyRight) {
        model->right_pressed = false;
        hid_hal_consumer_key_release(hid_tiktok->hid, HID_CONSUMER_VOLUME_INCREMENT);
    } else if(event->key == InputKeyOk) {
        model->ok_pressed = false;
    } else if(event->key == InputKeyBack) {
        model->back_mouse_pressed = false;
    }
}

static bool hid_tiktok_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    HidTikTok* hid_tiktok = context;
    bool consumed = false;

    // Flags for operations that need hundreds of ms of furi_delay_ms — these
    // must run OUTSIDE the with_view_model lock so the draw callback isn't
    // starved for the entire duration.
    bool do_cursor_reset = false;
    int  do_swipe        = 0; // -1 = up (next), +1 = down (prev), 0 = none

    with_view_model(
        hid_tiktok->view,
        HidTikTokModel * model,
        {
            if(event->type == InputTypePress) {
                hid_tiktok_process_press(hid_tiktok, model, event);
                if(model->connected && !model->is_cursor_set) {
                    model->is_cursor_set = true;
                    do_cursor_reset = true; // execute after lock is released
                }
                consumed = true;
            } else if(event->type == InputTypeRelease) {
                hid_tiktok_process_release(hid_tiktok, model, event);
                consumed = true;
            } else if(event->type == InputTypeShort) {
                if(event->key == InputKeyOk) {
                    // delays adjusted for emulation of a finger tap
                    hid_hal_mouse_press(hid_tiktok->hid, HID_MOUSE_BTN_LEFT);
                    furi_delay_ms(25);
                    hid_hal_mouse_release(hid_tiktok->hid, HID_MOUSE_BTN_LEFT);
                    furi_delay_ms(75);
                    hid_hal_mouse_press(hid_tiktok->hid, HID_MOUSE_BTN_LEFT);
                    furi_delay_ms(25);
                    hid_hal_mouse_release(hid_tiktok->hid, HID_MOUSE_BTN_LEFT);
                    consumed = true;
                } else if(event->key == InputKeyUp) {
                    if(hid_tiktok->hid->tiktok_scroll_mode == TikTokScrollGesture) {
                        do_swipe = 1; // execute after lock is released
                    } else {
                        // Emulate up swipe with the scroll wheel
                        hid_hal_mouse_scroll(hid_tiktok->hid, -12);
                        hid_hal_mouse_scroll(hid_tiktok->hid, -24);
                        hid_hal_mouse_scroll(hid_tiktok->hid, -38);
                        hid_hal_mouse_scroll(hid_tiktok->hid, -24);
                        hid_hal_mouse_scroll(hid_tiktok->hid, -12);
                    }
                    consumed = true;
                } else if(event->key == InputKeyDown) {
                    if(hid_tiktok->hid->tiktok_scroll_mode == TikTokScrollGesture) {
                        do_swipe = -1; // execute after lock is released
                    } else {
                        // Emulate down swipe with the scroll wheel
                        hid_hal_mouse_scroll(hid_tiktok->hid, 12);
                        hid_hal_mouse_scroll(hid_tiktok->hid, 24);
                        hid_hal_mouse_scroll(hid_tiktok->hid, 38);
                        hid_hal_mouse_scroll(hid_tiktok->hid, 24);
                        hid_hal_mouse_scroll(hid_tiktok->hid, 12);
                    }
                    consumed = true;
                } else if(event->key == InputKeyBack) {
                    // Pause
                    hid_hal_mouse_press(hid_tiktok->hid, HID_MOUSE_BTN_LEFT);
                    furi_delay_ms(50);
                    hid_hal_mouse_release(hid_tiktok->hid, HID_MOUSE_BTN_LEFT);
                    consumed = true;
                }
            } else if(event->type == InputTypeLong) {
                if(event->key == InputKeyBack) {
                    hid_hal_consumer_key_release_all(hid_tiktok->hid);
                    model->is_cursor_set = false;
                    consumed = false;
                }
            }
        },
        true);

    // Run delay-heavy BLE operations after releasing the model lock so the
    // draw callback isn't blocked for hundreds of milliseconds.
    if(do_cursor_reset) hid_tiktok_reset_cursor(hid_tiktok);
    if(do_swipe != 0)   hid_tiktok_gesture_swipe(hid_tiktok, do_swipe);

    return consumed;
}

HidTikTok* hid_tiktok_alloc(Hid* bt_hid) {
    HidTikTok* hid_tiktok = malloc(sizeof(HidTikTok));
    hid_tiktok->hid = bt_hid;
    hid_tiktok->view = view_alloc();
    view_set_context(hid_tiktok->view, hid_tiktok);
    view_allocate_model(hid_tiktok->view, ViewModelTypeLocking, sizeof(HidTikTokModel));
    view_set_draw_callback(hid_tiktok->view, hid_tiktok_draw_callback);
    view_set_input_callback(hid_tiktok->view, hid_tiktok_input_callback);

    return hid_tiktok;
}

void hid_tiktok_free(HidTikTok* hid_tiktok) {
    furi_assert(hid_tiktok);
    view_free(hid_tiktok->view);
    free(hid_tiktok);
}

View* hid_tiktok_get_view(HidTikTok* hid_tiktok) {
    furi_assert(hid_tiktok);
    return hid_tiktok->view;
}

void hid_tiktok_set_connected_status(HidTikTok* hid_tiktok, bool connected) {
    furi_assert(hid_tiktok);
    with_view_model(
        hid_tiktok->view,
        HidTikTokModel * model,
        {
            model->connected = connected;
            model->is_cursor_set = false;
        },
        true);
}

void hid_tiktok_set_mode(HidTikTok* hid_tiktok, bool gesture) {
    furi_assert(hid_tiktok);
    with_view_model(
        hid_tiktok->view, HidTikTokModel * model, { model->gesture_mode = gesture; }, true);
}
