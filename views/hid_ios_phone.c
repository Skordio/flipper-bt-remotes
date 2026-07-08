#include "hid_ios_phone.h"
#include "../hid.h"
#include <gui/elements.h>

#include "hid_icons.h"

#define TAG "HidIosPhone"

// Period of the cursor-move timer in ms. One mouse-move packet per tick; each
// BLE connection interval is ~15-30 ms, so 15 ms keeps the host fed without
// flooding it. The cursor moves while a d-pad direction is held, at either a
// constant rate or a slow-start ramp (ios_cursor_mode).
#define IOS_MOVE_TICK_MS 15
// Delay between swipe-chunk packets in ms. Chunk size is computed at swipe
// start from ios_swipe_speed_px_s so the user's px/sec setting controls
// drag speed at a fixed tick rate (HID mouse delta is int8, max 127).
#define IOS_SWIPE_STEP_MS 22
// Pixels of cursor motion emitted BEFORE the mouse button is pressed. iOS
// classifies touch-down as the start of a swipe vs. a tap based on whether the
// cursor is already moving when the button goes down.
#define IOS_SWIPE_LEAD_PX 14
// Horizontal swipes are scaled by the width:height ratio of a modern iPhone
// screen (19.5:9) so a "full swipe" left/right covers the same fraction of the
// screen as one up/down does. 9/19.5 == 6/13.
#define IOS_SWIPE_H_SCALE_NUM 6
#define IOS_SWIPE_H_SCALE_DEN 13

typedef enum {
    IosModeDefault = 0,
    IosModeSwipe,
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
    // Move timer drives the cursor in Default mode while a d-pad direction is
    // held (constant or ramping speed). Periodic; allocated once.
    FuriTimer* move_timer;
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
    // Default-mode move state. Only one direction can be moving at a time.
    bool     move_active;
    InputKey move_key;
    uint32_t move_start_tick;
    // Milli-px accumulator: each tick banks speed_px_s * IOS_MOVE_TICK_MS and
    // emits accum/1000 whole px, so slow ramp starts are genuinely slow
    // (sub-1 px/tick) instead of clamping to 1 px per tick.
    uint32_t move_accum_mpx;
    // Double-tap detection for the d-pad in Default mode: remember the last
    // direction release so a follow-up press within dbl_tap_window can be
    // promoted to a swipe.
    InputKey last_release_key;
    uint32_t last_release_tick;
    // Wall-clock of the most recent d-pad Press, used to require that the prior
    // press was a TAP (release - press < window) before treating the current
    // press as a double-tap. Without this, holding a key to cruise the cursor
    // and re-pressing within the double-tap window would fire an unintended swipe.
    uint32_t current_press_tick;
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
// finger-drag from right to left. Horizontal swipes are shortened by the
// screen's width:height ratio (see IOS_SWIPE_H_SCALE_*).
static void hid_ios_swipe_delta_for_key(Hid* hid, InputKey key, int* out_dx, int* out_dy) {
    int dist = (int)hid->ios_swipe_distance;
    int h_dist = dist * IOS_SWIPE_H_SCALE_NUM / IOS_SWIPE_H_SCALE_DEN;
    *out_dx = 0;
    *out_dy = 0;
    if(key == InputKeyRight)      *out_dx = -h_dist;
    else if(key == InputKeyLeft)  *out_dx = h_dist;
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

    // NOTE: transitions to Idle must post furi_timer_stop while STILL HOLDING
    // the model lock (inside the with_view_model block below). furi_timer_stop
    // is fire-and-forget (it queues a command; only furi_timer_free flushes),
    // so timer commands execute in FIFO post order. If the stop were posted
    // after releasing the lock, hid_ios_swipe_start could observe Idle, restart
    // the timer, and then have this callback's stale stop kill it — leaving
    // swipe_phase at Lead with no timer running (swipe mode bricked until the
    // view exits). Holding the lock guarantees any new swipe's stop+start
    // commands are queued after ours.
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
                model->swipe_phase = IosSwipePhaseReturn;
                model->swipe_remaining_x = -model->swipe_total_x;
                model->swipe_remaining_y = -model->swipe_total_y;
                break;
            case IosSwipePhaseReturn:
                // The return MUST use the same per-report chunk size as the
                // drag: iOS pointer acceleration scales with the delta in each
                // report, so a faster return (bigger reports) travels farther
                // on-screen than the drag did and overshoots the start point.
                if(model->swipe_remaining_x != 0) {
                    dx = hid_ios_chunk_step(model->swipe_remaining_x, model->swipe_chunk_px);
                    model->swipe_remaining_x -= dx;
                } else if(model->swipe_remaining_y != 0) {
                    dy = hid_ios_chunk_step(model->swipe_remaining_y, model->swipe_chunk_px);
                    model->swipe_remaining_y -= dy;
                } else {
                    hid_ios_swipe_reset_state(model);
                    furi_timer_stop(self->swipe_timer);
                }
                break;
            case IosSwipePhaseIdle:
            default:
                furi_timer_stop(self->swipe_timer);
                break;
            }
        },
        false);

    if(dx != 0 || dy != 0) hid_hal_mouse_move(self->hid, dx, dy);
    if(do_press) hid_hal_mouse_press(self->hid, HID_MOUSE_BTN_LEFT);
    if(do_release) hid_hal_mouse_release(self->hid, HID_MOUSE_BTN_LEFT);
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
    if(model->mode == IosModeSwipe)
        elements_multiline_text_aligned(canvas, 3, 22, AlignLeft, AlignTop, "Swipe mode");

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
// Move timer (Default mode)
// ---------------------------------------------------------------------------

static void hid_ios_phone_move_timer_cb(void* context) {
    furi_assert(context);
    HidIosPhone* self = context;

    int8_t dx = 0;
    int8_t dy = 0;
    bool   stop = false;

    with_view_model(
        self->view,
        HidIosPhoneModel * model,
        {
            if(!model->move_active) {
                stop = true;
            } else {
                // Current speed in px/s: the full cursor speed in Constant
                // mode; in Ramp mode, a linear climb from ios_ramp_start_pct
                // of it to all of it over ios_ramp_time_ms.
                uint32_t speed = self->hid->ios_cursor_speed_px_s;
                if(self->hid->ios_cursor_mode == IosCursorModeRamp) {
                    uint32_t elapsed = hid_ios_now_ms() - model->move_start_tick;
                    uint32_t ramp_ms = self->hid->ios_ramp_time_ms;
                    if(ramp_ms < 1) ramp_ms = 1;
                    if(elapsed < ramp_ms) {
                        uint32_t start = speed * self->hid->ios_ramp_start_pct / 100;
                        speed = start + (speed - start) * elapsed / ramp_ms;
                    }
                }

                // Bank this tick's travel in milli-px and emit the whole px.
                // Ticks that bank less than 1 px emit nothing (dx==dy==0).
                model->move_accum_mpx += speed * IOS_MOVE_TICK_MS;
                uint32_t delta = model->move_accum_mpx / 1000;
                if(delta > 127) delta = 127; // HID mouse delta is int8
                model->move_accum_mpx -= delta * 1000;

                switch(model->move_key) {
                case InputKeyRight: dx = (int8_t)delta;   break;
                case InputKeyLeft:  dx = (int8_t)-delta;  break;
                case InputKeyDown:  dy = (int8_t)delta;   break;
                case InputKeyUp:    dy = (int8_t)-delta;  break;
                default: stop = true; model->move_active = false; break;
                }
            }
        },
        false);

    if(stop) {
        furi_timer_stop(self->move_timer);
    } else if(dx != 0 || dy != 0) {
        hid_hal_mouse_move(self->hid, dx, dy);
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

static bool hid_ios_phone_input_callback(InputEvent* event, void* context) {
    furi_assert(context);
    HidIosPhone* self = context;
    bool consumed = false;

    // Long Back: clean up any held state and let the scene manager pop. We don't
    // consume the event so the view dispatcher's default Back handler fires.
    if(event->type == InputTypeLong && event->key == InputKeyBack) {
        hid_hal_mouse_release_all(self->hid);
        furi_timer_stop(self->move_timer);
        furi_timer_stop(self->swipe_timer);
        with_view_model(
            self->view,
            HidIosPhoneModel * model,
            {
                model->move_active = false;
                model->ok_pressed = false;
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

    // Back short = immediate Default <-> Swipe toggle.
    if(event->key == InputKeyBack && event->type == InputTypeShort) {
        bool swipe_held_button = false;
        with_view_model(
            self->view,
            HidIosPhoneModel * model,
            {
                model->mode = (model->mode == IosModeDefault) ? IosModeSwipe : IosModeDefault;
                // Reset transient state on mode change — including an in-flight
                // swipe, whose timer would otherwise keep emitting mouse reports
                // into the new mode.
                model->move_active = false;
                swipe_held_button = (model->swipe_phase == IosSwipePhaseDrag) ||
                                    (model->swipe_phase == IosSwipePhaseRelease);
                hid_ios_swipe_reset_state(model);
            },
            true);
        furi_timer_stop(self->move_timer);
        furi_timer_stop(self->swipe_timer);
        // A swipe aborted mid-Drag leaves the left button held down.
        if(swipe_held_button) hid_hal_mouse_release(self->hid, HID_MOUSE_BTN_LEFT);
        return true;
    }

    IosMode mode;
    with_view_model(
        self->view, HidIosPhoneModel * model, { mode = model->mode; }, false);

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
                    // Start moving in this direction.
                    model->move_active = true;
                    model->move_key = event->key;
                    model->move_start_tick = now;
                    model->move_accum_mpx = 0;
                }
                // Always record press start so the matching Release can decide
                // whether to arm a double-tap (short press only).
                model->current_press_tick = now;
            },
            true);
        if(do_swipe) {
            hid_ios_swipe_start(self, dx, dy);
        } else {
            furi_timer_stop(self->move_timer);
            furi_timer_start(self->move_timer, IOS_MOVE_TICK_MS);
        }
    } else if(event->type == InputTypeRelease) {
        uint32_t now = hid_ios_now_ms();
        with_view_model(
            self->view,
            HidIosPhoneModel * model,
            {
                if(model->move_active && model->move_key == event->key) {
                    model->move_active = false;
                }
                // Only arm a double-tap candidate if this was a TAP — a long
                // hold (e.g. the user cruised the cursor for a second) should
                // not pair with the next press as a double-tap.
                if((now - model->current_press_tick) < self->hid->ios_dbl_tap_window_ms) {
                    model->last_release_key = event->key;
                    model->last_release_tick = now;
                } else {
                    model->last_release_key = InputKeyMAX;
                }
            },
            true);
        furi_timer_stop(self->move_timer);
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
    furi_timer_stop(self->move_timer);
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

    self->move_timer =
        furi_timer_alloc(hid_ios_phone_move_timer_cb, FuriTimerTypePeriodic, self);
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
    furi_timer_stop(self->move_timer);
    furi_timer_free(self->move_timer);
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
