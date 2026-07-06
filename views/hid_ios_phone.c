#include "hid_ios_phone.h"
#include "../hid.h"
#include <gui/elements.h>

#include "hid_icons.h"

#define TAG "HidIosPhone"

// Period of the burst-move timer in ms. One mouse-move packet per tick; each BLE
// connection interval is ~15-30 ms, so 15 ms keeps the host fed without
// flooding it.
#define IOS_BURST_TICK_MS 15
// Total duration of a single burst at base speed. Tuned so a default-distance
// burst (480 px) plays out in ~825 ms — long enough to feel the deceleration
// curve clearly while keeping average velocity around 580 px/s.
#define IOS_BURST_DURATION_MS 825
// Delay between swipe-chunk packets in ms. Chunk size is computed at swipe
// start from ios_swipe_speed_px_s so the user's px/sec setting controls
// drag speed at a fixed tick rate (HID mouse delta is int8, max 127).
#define IOS_SWIPE_STEP_MS 22
// Pixels of cursor motion emitted BEFORE the mouse button is pressed. iOS
// classifies touch-down as the start of a swipe vs. a tap based on whether the
// cursor is already moving when the button goes down.
#define IOS_SWIPE_LEAD_PX 14

typedef enum {
    IosModeDefault = 0,
    IosModeSwipe,
    IosModeSlow,
} IosMode;

// Phases of a non-blocking swipe gesture. The swipe_timer ticks every
// IOS_SWIPE_STEP_MS and advances through these phases so the input thread is
// free to handle Long-Back (exit) and other events during the gesture.
typedef enum {
    IosSwipePhaseIdle = 0,
    IosSwipePhaseLead,    // cursor moving, button NOT yet pressed (iOS gesture priming)
    IosSwipePhaseDrag,    // button pressed; emit chunks until remaining is 0
    IosSwipePhaseRelease, // emit the button release on this tick
    IosSwipePhaseReturn,  // optional: drag back to the starting position (no button)
} IosSwipePhase;

struct HidIosPhone {
    View* view;
    Hid*  hid;
    // Burst timer drives the decelerating cursor move in Default mode while a
    // d-pad direction is held. Periodic; allocated once.
    FuriTimer* burst_timer;
    // Back single-tap action is deferred by dbl_tap_window_ms so a second short
    // press can override it as a double-tap. One-shot timer.
    FuriTimer* back_tap_timer;
    // Swipe state-machine timer; ticks every IOS_SWIPE_STEP_MS while a swipe
    // is in progress so the input thread isn't blocked for the gesture duration.
    FuriTimer* swipe_timer;
};

typedef struct {
    IosMode mode;
    bool    connected;
    // Direction-pressed flags for the draw. ok_pressed mirrors the held state of
    // the left mouse button — they're 1:1 in iOS-phone semantics.
    bool    up_pressed;
    bool    down_pressed;
    bool    left_pressed;
    bool    right_pressed;
    bool    ok_pressed;
    // Slow mode uses the same acceleration ramp as the Mouse view: 1 on press,
    // +1 per repeat, capped at 20, reset on release.
    uint8_t acceleration;
    // Default-mode burst state. Only one direction can be bursting at a time.
    bool     burst_active;
    InputKey burst_key;
    uint32_t burst_start_tick;
    // Double-tap detection for the d-pad in Default mode: remember the last
    // direction release so a follow-up press within dbl_tap_window can be
    // promoted to a swipe.
    InputKey last_release_key;
    uint32_t last_release_tick;
    // Wall-clock of the most recent d-pad Press, used to require that the prior
    // press was a TAP (release - press < window) before treating the current
    // press as a double-tap. Without this, holding a key through a full burst
    // and re-pressing within the double-tap window would fire an unintended swipe.
    uint32_t current_press_tick;
    // Back single-tap is deferred so a second short press promotes it to a
    // double-tap. The flag mirrors the back_tap_timer's pending state for the
    // input callback to test from inside the model lock.
    bool back_short_pending;
    // Non-blocking swipe state. swipe_phase != Idle means a swipe is mid-flight
    // and new swipe triggers are ignored.
    IosSwipePhase swipe_phase;
    int           swipe_remaining_x; // px left to emit on the x axis this phase
    int           swipe_remaining_y; // px left to emit on the y axis this phase
    int           swipe_total_x;     // saved for the return leg's mirror move
    int           swipe_total_y;
    int           swipe_chunk_px;    // per-tick chunk derived from ios_swipe_speed_px_s
} HidIosPhoneModel;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static uint32_t hid_ios_now_ms(void) {
    uint32_t freq = furi_kernel_get_tick_frequency();
    if(freq == 0) freq = 1000;
    return (uint32_t)((uint64_t)furi_get_tick() * 1000u / freq);
}

// Clamp a signed delta to one chunk step (HID mouse delta is int8, max 127).
static int8_t hid_ios_chunk_step(int remaining, int chunk) {
    if(chunk > 127) chunk = 127;
    if(chunk < 1) chunk = 1;
    if(remaining > chunk) return (int8_t)chunk;
    if(remaining < -chunk) return (int8_t)-chunk;
    return (int8_t)remaining;
}

// Abort an in-flight swipe and reset the state machine. Caller is responsible
// for stopping the swipe_timer and releasing the mouse button as appropriate.
// Must hold the model lock.
static void hid_ios_swipe_reset_state(HidIosPhoneModel* model) {
    model->swipe_phase = IosSwipePhaseIdle;
    model->swipe_remaining_x = 0;
    model->swipe_remaining_y = 0;
    model->swipe_total_x = 0;
    model->swipe_total_y = 0;
}

// Map a d-pad key + the per-profile swipe distance to the (dx, dy) the cursor
// should travel during a swipe. The drag direction is OPPOSITE the pressed
// key — pressing Right intends "swipe right" on the phone, which is a
// finger-drag from right to left.
static void hid_ios_swipe_delta_for_key(Hid* hid, InputKey key, int* out_dx, int* out_dy) {
    int dist = (int)hid->ios_swipe_distance;
    *out_dx = 0;
    *out_dy = 0;
    if(key == InputKeyRight)      *out_dx = -dist;
    else if(key == InputKeyLeft)  *out_dx = dist;
    else if(key == InputKeyDown)  *out_dy = -dist;
    else if(key == InputKeyUp)    *out_dy = dist;
}

// Kick off a non-blocking swipe: start in Lead phase so the cursor begins
// moving before the mouse button is pressed (iOS reads stationary touch-down
// as a tap, not a swipe). The press itself happens at the end of the first
// timer tick, after that tick's lead motion. Returns false if a swipe is
// already in progress.
static bool hid_ios_swipe_start(HidIosPhone* self, int dx, int dy) {
    // px/sec -> px/tick; ceil so the slowest setting still emits at least 1 px
    // per tick (otherwise the swipe would stall on round-down).
    int chunk = ((int)self->hid->ios_swipe_speed_px_s * IOS_SWIPE_STEP_MS + 999) / 1000;
    if(chunk < 1) chunk = 1;
    if(chunk > 127) chunk = 127;

    bool busy = false;
    with_view_model(
        self->view,
        HidIosPhoneModel * model,
        {
            if(model->swipe_phase != IosSwipePhaseIdle) {
                busy = true;
            } else {
                model->swipe_phase = IosSwipePhaseLead;
                model->swipe_remaining_x = dx;
                model->swipe_remaining_y = dy;
                model->swipe_total_x = dx;
                model->swipe_total_y = dy;
                model->swipe_chunk_px = chunk;
            }
        },
        false);
    if(busy) return false;
    furi_timer_stop(self->swipe_timer);
    furi_timer_start(self->swipe_timer, IOS_SWIPE_STEP_MS);
    return true;
}

// One tick of the swipe state machine. Each tick emits at most one mouse-move
// plus optionally one button event so the input thread sees long-Back /
// mode-toggle events promptly. State transitions:
//   Lead    -> emit a short lead move (capped so >=1 px is left for Drag),
//              press the mouse button after the move, advance to Drag.
//   Drag    -> emit one chunk; when remaining_x and remaining_y are both 0,
//              advance to Release.
//   Release -> emit mouse release; if Return-to-Start is on, prime the mirror
//              move and advance to Return; otherwise Idle + stop timer.
//   Return  -> emit one chunk (no button); when remaining is 0, Idle + stop.
// Per-tick BLE ordering is move -> press -> release, so in Lead the cursor is
// already in motion at touch-down.
static void hid_ios_phone_swipe_timer_cb(void* context) {
    furi_assert(context);
    HidIosPhone* self = context;

    int8_t dx = 0, dy = 0;
    bool   do_press    = false;
    bool   do_release  = false;
    bool   stop_timer  = false;

    with_view_model(
        self->view,
        HidIosPhoneModel * model,
        {
            switch(model->swipe_phase) {
            case IosSwipePhaseLead: {
                int sign_x = (model->swipe_remaining_x > 0) ? 1 :
                             (model->swipe_remaining_x < 0) ? -1 : 0;
                int sign_y = (model->swipe_remaining_y > 0) ? 1 :
                             (model->swipe_remaining_y < 0) ? -1 : 0;
                int lead = IOS_SWIPE_LEAD_PX;
                if(sign_x != 0) {
                    int max_lead = (model->swipe_remaining_x * sign_x) - 1;
                    if(max_lead < 1) max_lead = 1;
                    if(lead > max_lead) lead = max_lead;
                    int amt = lead * sign_x;
                    dx = (int8_t)amt;
                    model->swipe_remaining_x -= amt;
                } else if(sign_y != 0) {
                    int max_lead = (model->swipe_remaining_y * sign_y) - 1;
                    if(max_lead < 1) max_lead = 1;
                    if(lead > max_lead) lead = max_lead;
                    int amt = lead * sign_y;
                    dy = (int8_t)amt;
                    model->swipe_remaining_y -= amt;
                }
                do_press = true;
                model->swipe_phase = IosSwipePhaseDrag;
                break;
            }
            case IosSwipePhaseDrag:
                if(model->swipe_remaining_x != 0) {
                    dx = hid_ios_chunk_step(model->swipe_remaining_x, model->swipe_chunk_px);
                    model->swipe_remaining_x -= dx;
                } else if(model->swipe_remaining_y != 0) {
                    dy = hid_ios_chunk_step(model->swipe_remaining_y, model->swipe_chunk_px);
                    model->swipe_remaining_y -= dy;
                } else {
                    model->swipe_phase = IosSwipePhaseRelease;
                }
                break;
            case IosSwipePhaseRelease:
                do_release = true;
                if(self->hid->ios_swipe_return_to_start) {
                    model->swipe_phase = IosSwipePhaseReturn;
                    model->swipe_remaining_x = -model->swipe_total_x;
                    model->swipe_remaining_y = -model->swipe_total_y;
                } else {
                    hid_ios_swipe_reset_state(model);
                    stop_timer = true;
                }
                break;
            case IosSwipePhaseReturn:
                if(model->swipe_remaining_x != 0) {
                    dx = hid_ios_chunk_step(model->swipe_remaining_x, model->swipe_chunk_px);
                    model->swipe_remaining_x -= dx;
                } else if(model->swipe_remaining_y != 0) {
                    dy = hid_ios_chunk_step(model->swipe_remaining_y, model->swipe_chunk_px);
                    model->swipe_remaining_y -= dy;
                } else {
                    hid_ios_swipe_reset_state(model);
                    stop_timer = true;
                }
                break;
            case IosSwipePhaseIdle:
            default:
                stop_timer = true;
                break;
            }
        },
        false);

    if(dx != 0 || dy != 0) hid_hal_mouse_move(self->hid, dx, dy);
    if(do_press) hid_hal_mouse_press(self->hid, HID_MOUSE_BTN_LEFT);
    if(do_release) hid_hal_mouse_release(self->hid, HID_MOUSE_BTN_LEFT);
    if(stop_timer) furi_timer_stop(self->swipe_timer);
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

static void hid_ios_phone_draw_callback(Canvas* canvas, void* context) {
    furi_assert(context);
    HidIosPhoneModel* model = context;

    // Header
#ifdef HID_TRANSPORT_BLE
    if(model->connected) {
        canvas_draw_icon(canvas, 0, 0, &I_Ble_connected_15x15);
    } else {
        canvas_draw_icon(canvas, 0, 0, &I_Ble_disconnected_15x15);
    }
#endif

    canvas_set_font(canvas, FontPrimary);
    elements_multiline_text_aligned(canvas, 17, 3, AlignLeft, AlignTop, "iOS Phone");
    canvas_set_font(canvas, FontSecondary);

    // Mode indicator just under the title.
    const char* mode_label = NULL;
    if(model->mode == IosModeSwipe) mode_label = "Swipe mode";
    else if(model->mode == IosModeSlow) mode_label = "Slow mode";
    if(mode_label) elements_multiline_text_aligned(canvas, 3, 22, AlignLeft, AlignTop, mode_label);

    if(model->ok_pressed) {
        elements_multiline_text_aligned(canvas, 0, 62, AlignLeft, AlignBottom, "Holding...");
    } else {
        canvas_draw_icon(canvas, 0, 54, &I_Pin_back_arrow_10x8);
        elements_multiline_text_aligned(canvas, 13, 62, AlignLeft, AlignBottom, "Hold to exit");
    }

    // D-pad illustration (same shape as the Mouse view).
    canvas_draw_icon(canvas, 58, 3, &I_OutCircles_70x51);

    // Up
    if(model->up_pressed) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 68, 6, &I_S_UP_31x15);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 80, 8, &I_Pin_arrow_up_7x9);
    canvas_set_color(canvas, ColorBlack);

    // Down
    if(model->down_pressed) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 68, 36, &I_S_DOWN_31x15);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 80, 40, &I_Pin_arrow_down_7x9);
    canvas_set_color(canvas, ColorBlack);

    // Left
    if(model->left_pressed) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 61, 13, &I_S_LEFT_15x31);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 63, 25, &I_Pin_arrow_left_9x7);
    canvas_set_color(canvas, ColorBlack);

    // Right
    if(model->right_pressed) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 91, 13, &I_S_RIGHT_15x31);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 95, 25, &I_Pin_arrow_right_9x7);
    canvas_set_color(canvas, ColorBlack);

    // OK indicator
    if(model->ok_pressed) {
        canvas_set_bitmap_mode(canvas, true);
        canvas_draw_icon(canvas, 74, 19, &I_Pressed_Button_19x19);
        canvas_set_bitmap_mode(canvas, false);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_icon(canvas, 79, 24, &I_Left_mouse_icon_9x9);
    canvas_set_color(canvas, ColorBlack);
}

// ---------------------------------------------------------------------------
// Burst timer (Default mode)
// ---------------------------------------------------------------------------

static void hid_ios_phone_burst_timer_cb(void* context) {
    furi_assert(context);
    HidIosPhone* self = context;

    int8_t dx = 0;
    int8_t dy = 0;
    bool   stop = false;

    with_view_model(
        self->view,
        HidIosPhoneModel * model,
        {
            if(!model->burst_active) {
                stop = true;
            } else {
                // Symmetric triangle velocity profile: ramp from 0.5*peak at
                // t=0 up to peak at t=T/2, then back down to 0.5*peak at t=T.
                // Past T, the `delta < 1` clamp below takes over so a held key
                // keeps producing a slow precision crawl until release. Average
                // height is 0.75*peak, so peak = (4*dist) / (3*N) keeps the
                // total area equal to ios_burst_distance.
                uint32_t elapsed = hid_ios_now_ms() - model->burst_start_tick;
                int32_t dist = (int32_t)self->hid->ios_burst_distance;
                int32_t n_ticks = IOS_BURST_DURATION_MS / IOS_BURST_TICK_MS;
                if(n_ticks < 1) n_ticks = 1;
                int32_t peak = (4 * dist + 3 * n_ticks - 1) / (3 * n_ticks); // round up
                int32_t delta = 0;
                if(elapsed < IOS_BURST_DURATION_MS) {
                    int32_t T = (int32_t)IOS_BURST_DURATION_MS;
                    if((int32_t)elapsed * 2 <= T) {
                        // First half: 0.5*peak -> peak.
                        delta = peak * (T + 2 * (int32_t)elapsed) / (2 * T);
                    } else {
                        // Second half: peak -> 0.5*peak.
                        delta = peak * (3 * T - 2 * (int32_t)elapsed) / (2 * T);
                    }
                }
                if(delta < 1) delta = 1;
                if(delta > 127) delta = 127;

                switch(model->burst_key) {
                case InputKeyRight: dx = (int8_t)delta;   break;
                case InputKeyLeft:  dx = (int8_t)-delta;  break;
                case InputKeyDown:  dy = (int8_t)delta;   break;
                case InputKeyUp:    dy = (int8_t)-delta;  break;
                default: stop = true; model->burst_active = false; break;
                }
            }
        },
        false);

    if(stop) {
        furi_timer_stop(self->burst_timer);
    } else if(dx != 0 || dy != 0) {
        hid_hal_mouse_move(self->hid, dx, dy);
    }
}

// ---------------------------------------------------------------------------
// Back tap timer (single-tap deferred action)
// ---------------------------------------------------------------------------

// Apply a single-tap Back: enter Swipe from Default, exit to Default from any
// non-Default mode. Guarded by back_short_pending so a racing double-tap path
// (which clears the flag before the timer can run) can't double-toggle the mode.
static void hid_ios_phone_back_tap_timer_cb(void* context) {
    furi_assert(context);
    HidIosPhone* self = context;
    bool fire = false;
    with_view_model(
        self->view,
        HidIosPhoneModel * model,
        {
            // If the input thread already promoted to a double-tap it cleared
            // back_short_pending; the queued callback then no-ops here.
            if(model->back_short_pending) {
                model->back_short_pending = false;
                model->mode = (model->mode == IosModeDefault) ? IosModeSwipe : IosModeDefault;
                model->burst_active = false;
                model->acceleration = 0;
                fire = true;
            }
        },
        true);
    if(fire) furi_timer_stop(self->burst_timer);
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

// Slow mode input handling: forwards to the standalone Mouse view's shared
// helper so behavior stays in lockstep with the regular Mouse remote.
static void hid_ios_slow_process(HidIosPhone* self, InputEvent* event) {
    with_view_model(
        self->view,
        HidIosPhoneModel * model,
        {
            hid_mouse_dpad_process(
                self->hid, event, &model->acceleration,
                &model->up_pressed, &model->down_pressed,
                &model->left_pressed, &model->right_pressed);
        },
        true);
}

static bool hid_ios_phone_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    HidIosPhone* self = context;
    bool consumed = false;

    // Long Back: clean up any held state and let the scene manager pop. We don't
    // consume the event so the view dispatcher's default Back handler fires.
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        hid_hal_mouse_release_all(self->hid);
        furi_timer_stop(self->burst_timer);
        furi_timer_stop(self->back_tap_timer);
        furi_timer_stop(self->swipe_timer);
        with_view_model(
            self->view,
            HidIosPhoneModel * model,
            {
                model->burst_active = false;
                model->ok_pressed = false;
                model->back_short_pending = false;
                hid_ios_swipe_reset_state(model);
            },
            true);
        return false;
    }

    // OK = mouse left button while held. Same in every mode.
    if(event->key == InputKeyOk) {
        if(event->type == InputTypePress) {
            hid_hal_mouse_press(self->hid, HID_MOUSE_BTN_LEFT);
            with_view_model(
                self->view, HidIosPhoneModel * model, { model->ok_pressed = true; }, true);
            return true;
        } else if(event->type == InputTypeRelease) {
            hid_hal_mouse_release(self->hid, HID_MOUSE_BTN_LEFT);
            with_view_model(
                self->view, HidIosPhoneModel * model, { model->ok_pressed = false; }, true);
            return true;
        }
        // InputTypeShort / Long on OK are no-ops — Press/Release already covered it.
        return true;
    }

    // Back short = mode toggle, single vs double tap distinguished by a deferred
    // timer fire (see hid_ios_phone_back_tap_timer_cb).
    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        bool fire_double = false;
        with_view_model(
            self->view,
            HidIosPhoneModel * model,
            {
                if(model->back_short_pending) {
                    // Second tap inside the window — promote to double-tap.
                    model->back_short_pending = false;
                    fire_double = true;
                } else {
                    model->back_short_pending = true;
                }
            },
            false);
        if(fire_double) {
            furi_timer_stop(self->back_tap_timer);
            with_view_model(
                self->view,
                HidIosPhoneModel * model,
                {
                    // Same "Default <-> mode" symmetry as the single-tap path:
                    // a double-tap from Default enters Slow; from any non-Default
                    // mode (Slow or Swipe) it returns to Default.
                    model->mode = (model->mode == IosModeDefault) ? IosModeSlow : IosModeDefault;
                    // Reset transient state on mode change.
                    model->burst_active = false;
                    model->acceleration = 0;
                },
                true);
            furi_timer_stop(self->burst_timer);
        } else {
            furi_timer_stop(self->back_tap_timer);
            uint32_t window = self->hid->ios_dbl_tap_window_ms;
            if(window < 60) window = 60;
            furi_timer_start(self->back_tap_timer, window);
        }
        return true;
    }

    // Slow mode — let the standalone Mouse remote's input model handle the d-pad.
    IosMode mode;
    with_view_model(
        self->view, HidIosPhoneModel * model, { mode = model->mode; }, false);

    if(mode == IosModeSlow) {
        if(event->key == InputKeyRight || event->key == InputKeyLeft ||
           event->key == InputKeyUp || event->key == InputKeyDown) {
            hid_ios_slow_process(self, event);
            return true;
        }
        return false;
    }

    // Default and Swipe modes share most of the d-pad handling. We dispatch on
    // (mode, event type) below.
    if(event->key != InputKeyRight && event->key != InputKeyLeft &&
       event->key != InputKeyUp && event->key != InputKeyDown) {
        return false;
    }

    // Set the press-indicator flags on Press/Release. These don't depend on mode.
    if(event->type == InputTypePress || event->type == InputTypeRelease) {
        bool down = (event->type == InputTypePress);
        with_view_model(
            self->view,
            HidIosPhoneModel * model,
            {
                if(event->key == InputKeyRight) model->right_pressed = down;
                if(event->key == InputKeyLeft)  model->left_pressed  = down;
                if(event->key == InputKeyUp)    model->up_pressed    = down;
                if(event->key == InputKeyDown)  model->down_pressed  = down;
            },
            true);
        consumed = true;
    }

    if(mode == IosModeSwipe) {
        // Swipe mode: a single press of a direction kicks off a non-blocking
        // swipe. We trigger on Press so the user feels immediate response;
        // hid_ios_swipe_start no-ops if a previous swipe is still mid-flight.
        if(event->type == InputTypePress) {
            int dx = 0, dy = 0;
            hid_ios_swipe_delta_for_key(self->hid, event->key, &dx, &dy);
            hid_ios_swipe_start(self, dx, dy);
        }
        return consumed;
    }

    // Default mode below.
    if(event->type == InputTypePress) {
        // Check double-tap: same direction released < dbl_tap_window ago.
        bool   do_swipe = false;
        int    dx = 0, dy = 0;
        uint32_t now = hid_ios_now_ms();
        with_view_model(
            self->view,
            HidIosPhoneModel * model,
            {
                if(self->hid->ios_dbl_tap_swipe &&
                   model->last_release_key == event->key &&
                   (now - model->last_release_tick) < self->hid->ios_dbl_tap_window_ms) {
                    do_swipe = true;
                    hid_ios_swipe_delta_for_key(self->hid, event->key, &dx, &dy);
                    // Clear so a triple-tap doesn't chain swipes.
                    model->last_release_key = InputKeyMAX;
                } else {
                    // Start a fresh burst.
                    model->burst_active = true;
                    model->burst_key = event->key;
                    model->burst_start_tick = now;
                }
                // Always record press start so the matching Release can decide
                // whether to arm a double-tap (short press only).
                model->current_press_tick = now;
            },
            true);
        if(do_swipe) {
            hid_ios_swipe_start(self, dx, dy);
        } else {
            furi_timer_stop(self->burst_timer);
            furi_timer_start(self->burst_timer, IOS_BURST_TICK_MS);
        }
    } else if(event->type == InputTypeRelease) {
        uint32_t now = hid_ios_now_ms();
        with_view_model(
            self->view,
            HidIosPhoneModel * model,
            {
                if(model->burst_active && model->burst_key == event->key) {
                    model->burst_active = false;
                }
                // Only arm a double-tap candidate if this was a TAP — a long
                // hold (e.g. the user kept the burst pinned for a second)
                // should not pair with the next press as a double-tap.
                if((now - model->current_press_tick) < self->hid->ios_dbl_tap_window_ms) {
                    model->last_release_key = event->key;
                    model->last_release_tick = now;
                } else {
                    model->last_release_key = InputKeyMAX;
                }
            },
            true);
        furi_timer_stop(self->burst_timer);
    }

    return consumed;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

static void hid_ios_phone_exit_callback(void* context) {
    furi_assert(context);
    HidIosPhone* self = context;
    // Releasing the view: drop any held state so leaving via long-Back doesn't
    // leave the mouse button stuck on the host.
    hid_hal_mouse_release_all(self->hid);
    furi_timer_stop(self->burst_timer);
    furi_timer_stop(self->back_tap_timer);
    furi_timer_stop(self->swipe_timer);
}

HidIosPhone* hid_ios_phone_alloc(Hid* bt_hid) {
    HidIosPhone* self = malloc(sizeof(HidIosPhone));
    self->hid  = bt_hid;
    self->view = view_alloc();

    view_set_context(self->view, self);
    view_allocate_model(self->view, ViewModelTypeLocking, sizeof(HidIosPhoneModel));
    view_set_draw_callback(self->view, hid_ios_phone_draw_callback);
    view_set_input_callback(self->view, hid_ios_phone_input_callback);
    view_set_exit_callback(self->view, hid_ios_phone_exit_callback);

    self->burst_timer =
        furi_timer_alloc(hid_ios_phone_burst_timer_cb, FuriTimerTypePeriodic, self);
    self->back_tap_timer =
        furi_timer_alloc(hid_ios_phone_back_tap_timer_cb, FuriTimerTypeOnce, self);
    self->swipe_timer =
        furi_timer_alloc(hid_ios_phone_swipe_timer_cb, FuriTimerTypePeriodic, self);

    with_view_model(
        self->view,
        HidIosPhoneModel * model,
        {
            model->mode = IosModeDefault;
            model->last_release_key = InputKeyMAX;
        },
        true);

    return self;
}

void hid_ios_phone_free(HidIosPhone* self) {
    furi_assert(self);
    furi_timer_stop(self->burst_timer);
    furi_timer_free(self->burst_timer);
    furi_timer_stop(self->back_tap_timer);
    furi_timer_free(self->back_tap_timer);
    furi_timer_stop(self->swipe_timer);
    furi_timer_free(self->swipe_timer);
    view_free(self->view);
    free(self);
}

View* hid_ios_phone_get_view(HidIosPhone* self) {
    furi_assert(self);
    return self->view;
}

void hid_ios_phone_set_connected_status(HidIosPhone* self, bool connected) {
    furi_assert(self);
    with_view_model(
        self->view, HidIosPhoneModel * model, { model->connected = connected; }, true);
}
