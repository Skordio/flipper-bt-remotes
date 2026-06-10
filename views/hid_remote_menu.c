#include "hid_remote_menu.h"
#include <gui/canvas.h>
#include <gui/elements.h>
#include <input/input.h>
#include <furi.h>

#define TAG "HidRemoteMenu"

#define REMOTE_MENU_MAX_ITEMS 32 // 16 fixed items + 16 active custom remotes
#define REMOTE_MENU_VISIBLE   5   // rows shown on screen at once
#define REMOTE_MENU_ROW_H     11  // pixel height per row
#define REMOTE_MENU_Y_START   3   // y of first visible row's baseline area

struct HidRemoteMenu {
    View* view;

    HidRemoteMenuSelectCallback  select_cb;
    void*                        select_ctx;
    HidRemoteMenuReorderCallback reorder_cb;
    void*                        reorder_ctx;
};

typedef struct {
    const char* labels[REMOTE_MENU_MAX_ITEMS];
    uint8_t     indices[REMOTE_MENU_MAX_ITEMS]; // BtRemotesStartIndex per visual position
    uint8_t     count;
    uint8_t     fixed_count;    // items at end of list that cannot be reordered
    uint8_t     divider_value;  // index value of the visibility divider item; 0xFF = disabled
    uint8_t     divider_pos;    // current visual position of divider item; 0xFF if not found
    uint8_t     cursor;         // current visual position (0-based)
    bool        reorder_mode;   // true while an item is being dragged
} HidRemoteMenuModel;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static uint8_t clamp8(uint8_t v, uint8_t lo, uint8_t hi) {
    if(v < lo) return lo;
    if(v > hi) return hi;
    return v;
}

// Number of items navigable/visible in normal mode.
// In reorder mode, or when the divider is disabled, returns count.
static uint8_t hid_remote_menu_eff_count(const HidRemoteMenuModel* model) {
    if(!model->reorder_mode &&
       model->divider_value != 0xFF &&
       model->divider_pos != 0xFF) {
        return model->divider_pos + 1;
    }
    return model->count;
}

static void swap_items(HidRemoteMenuModel* model, uint8_t a, uint8_t b) {
    if(a >= model->count || b >= model->count) return;
    const char* tmp_label = model->labels[a];
    uint8_t     tmp_idx   = model->indices[a];
    model->labels[a]  = model->labels[b];
    model->indices[a] = model->indices[b];
    model->labels[b]  = tmp_label;
    model->indices[b] = tmp_idx;
    // Keep divider_pos tracking correct as the divider item moves
    if(model->divider_value != 0xFF && model->divider_pos != 0xFF) {
        if(model->divider_pos == a)
            model->divider_pos = b;
        else if(model->divider_pos == b)
            model->divider_pos = a;
    }
}

// ---------------------------------------------------------------------------
// Draw
// ---------------------------------------------------------------------------

static void hid_remote_menu_draw_cb(Canvas* canvas, void* model_ptr) {
    HidRemoteMenuModel* model = model_ptr;

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);

    if(model->count == 0) return;

    // First fixed item's absolute index (= count - fixed_count)
    uint8_t fixed_start = (model->fixed_count <= model->count)
                              ? (model->count - model->fixed_count)
                              : 0;

    bool has_divider = (model->divider_value != 0xFF && model->divider_pos != 0xFF);

    // Number of items to render this frame
    uint8_t render_count = model->reorder_mode ? model->count : hid_remote_menu_eff_count(model);

    // Display cursor — clamped to the rendered range so hidden items never highlight
    uint8_t disp_cursor = (render_count > 0) ? clamp8(model->cursor, 0, render_count - 1) : 0;

    // Scroll offset: keep cursor ~2 rows from the top
    int top = (int)disp_cursor - 2;
    if(top < 0) top = 0;
    if(top > (int)render_count - REMOTE_MENU_VISIBLE)
        top = (int)render_count - REMOTE_MENU_VISIBLE;
    if(top < 0) top = 0;

    for(int vi = 0; vi < REMOTE_MENU_VISIBLE; vi++) {
        int item = top + vi;
        if(item >= (int)render_count) break;

        int y = REMOTE_MENU_Y_START + vi * REMOTE_MENU_ROW_H;

        // Thin separator line before the first fixed (pinned) item
        if(model->fixed_count > 0 && (uint8_t)item == fixed_start) {
            canvas_draw_line(canvas, 0, y - 2, 125, y - 2);
        }

        // Thin separator line after the visibility divider item (reorder mode only)
        // Drawn before the first hidden item so it appears between divider and hidden zone
        if(model->reorder_mode && has_divider &&
           model->divider_pos != 0xFF &&
           (uint8_t)item == model->divider_pos + 1) {
            canvas_draw_line(canvas, 0, y - 2, 125, y - 2);
        }

        bool is_cursor = ((uint8_t)item == disp_cursor);

        if(is_cursor) {
            canvas_draw_box(canvas, 0, y - 1, 128, REMOTE_MENU_ROW_H);
            canvas_set_color(canvas, ColorWhite);
        }

        if(is_cursor && model->reorder_mode) {
            canvas_draw_str(canvas, 2, y + 8, ">");
            canvas_draw_str(canvas, 11, y + 8, model->labels[item]);
        } else {
            canvas_draw_str(canvas, 4, y + 8, model->labels[item]);
        }

        if(is_cursor) {
            canvas_set_color(canvas, ColorBlack);
        }
    }

    // Scroll indicator on the right edge
    if(render_count > REMOTE_MENU_VISIBLE) {
        uint8_t bar_h = 64 * REMOTE_MENU_VISIBLE / render_count;
        uint8_t bar_y = 64 * (uint8_t)top / render_count;
        if(bar_y + bar_h > 63) bar_y = 63 - bar_h;
        canvas_draw_line(canvas, 127, 0, 127, 63);
        canvas_draw_box(canvas, 126, bar_y, 2, bar_h > 0 ? bar_h : 1);
    }
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------

static bool hid_remote_menu_input_cb(InputEvent* event, void* context) {
    furi_assert(context);
    HidRemoteMenu* menu = context;
    bool consumed = false;

    bool fire_select  = false;
    bool fire_reorder = false;
    uint8_t fired_index = 0;

    with_view_model(
        menu->view,
        HidRemoteMenuModel * model,
        {
            // Upper bound for the reorderable zone (exclusive)
            uint8_t reorder_max = (model->fixed_count <= model->count)
                                      ? (model->count - model->fixed_count)
                                      : 0;

            // In normal mode, snap cursor into the visible range before acting.
            // This corrects any stale cursor value left from a previous reorder.
            if(!model->reorder_mode && model->count > 0) {
                uint8_t eff = hid_remote_menu_eff_count(model);
                if(eff > 0 && model->cursor >= eff) {
                    model->cursor = eff - 1;
                }
            }

            if(event->key == InputKeyUp &&
               (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
                if(model->reorder_mode && model->cursor > 0) {
                    swap_items(model, model->cursor, model->cursor - 1);
                }
                if(model->cursor > 0) model->cursor--;
                consumed = true;

            } else if(event->key == InputKeyDown &&
                      (event->type == InputTypeShort || event->type == InputTypeRepeat)) {
                if(model->reorder_mode) {
                    // Don't allow dragging into the fixed zone
                    if(model->cursor + 1 < reorder_max) {
                        swap_items(model, model->cursor, model->cursor + 1);
                        model->cursor++;
                    }
                } else {
                    // In normal mode, navigation stops at the divider (or end of list)
                    uint8_t eff = hid_remote_menu_eff_count(model);
                    uint8_t max_cursor = (eff > 0) ? (eff - 1) : 0;
                    if(model->cursor < max_cursor) model->cursor++;
                }
                consumed = true;

            } else if(event->key == InputKeyOk && event->type == InputTypeLong) {
                // Only enter reorder mode if there are reorderable items and
                // the cursor is currently on a reorderable item
                if(!model->reorder_mode && reorder_max > 0 && model->cursor < reorder_max) {
                    model->reorder_mode = true;
                    consumed = true;
                }

            } else if(event->key == InputKeyOk && event->type == InputTypeShort) {
                if(model->reorder_mode) {
                    model->reorder_mode = false;
                    fire_reorder = true;
                } else {
                    fire_select = true;
                    fired_index = model->indices[model->cursor];
                }
                consumed = true;

            } else if(event->key == InputKeyBack && event->type == InputTypeShort) {
                if(model->reorder_mode) {
                    model->reorder_mode = false;
                    fire_reorder = true;
                    consumed = true; // don't let Back navigate away
                }
                // else: return false so scene's Back handler fires
            }
        },
        true);

    // Fire callbacks outside the model lock
    if(fire_select && menu->select_cb) {
        menu->select_cb(menu->select_ctx, fired_index);
    }

    if(fire_reorder && menu->reorder_cb) {
        // Only pass the reorderable portion — exclude fixed items at the end.
        uint8_t order[REMOTE_MENU_MAX_ITEMS];
        uint8_t reorderable_count = 0;
        with_view_model(
            menu->view,
            HidRemoteMenuModel * m,
            {
                reorderable_count = (m->fixed_count <= m->count)
                                        ? (m->count - m->fixed_count)
                                        : 0;
                for(uint8_t i = 0; i < reorderable_count; i++) order[i] = m->indices[i];
            },
            false);
        menu->reorder_cb(menu->reorder_ctx, order, reorderable_count);
    }

    return consumed;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

HidRemoteMenu* hid_remote_menu_alloc(void) {
    HidRemoteMenu* menu = malloc(sizeof(HidRemoteMenu));
    menu->view = view_alloc();
    view_set_context(menu->view, menu);
    view_allocate_model(menu->view, ViewModelTypeLocking, sizeof(HidRemoteMenuModel));
    view_set_draw_callback(menu->view, hid_remote_menu_draw_cb);
    view_set_input_callback(menu->view, hid_remote_menu_input_cb);

    with_view_model(
        menu->view,
        HidRemoteMenuModel * model,
        {
            model->count        = 0;
            model->fixed_count  = 0;
            model->divider_value = 0xFF;
            model->divider_pos   = 0xFF;
            model->cursor       = 0;
            model->reorder_mode = false;
        },
        false);

    menu->select_cb   = NULL;
    menu->select_ctx  = NULL;
    menu->reorder_cb  = NULL;
    menu->reorder_ctx = NULL;

    return menu;
}

void hid_remote_menu_free(HidRemoteMenu* menu) {
    furi_assert(menu);
    view_free(menu->view);
    free(menu);
}

View* hid_remote_menu_get_view(HidRemoteMenu* menu) {
    furi_assert(menu);
    return menu->view;
}

void hid_remote_menu_set_items(
    HidRemoteMenu*            menu,
    const BtRemotesMenuEntry* default_table,
    const uint8_t*            order,
    uint8_t                   count,
    uint8_t                   fixed_count) {
    furi_assert(menu);
    furi_assert(default_table);
    furi_assert(order);
    if(count > REMOTE_MENU_MAX_ITEMS) count = REMOTE_MENU_MAX_ITEMS;
    if(fixed_count > count) fixed_count = count;

    with_view_model(
        menu->view,
        HidRemoteMenuModel * model,
        {
            model->count        = count;
            model->fixed_count  = fixed_count;
            model->reorder_mode = false;

            for(uint8_t i = 0; i < count; i++) {
                uint8_t idx_val = order[i];
                model->indices[i] = idx_val;
                // Find the matching label in the default table
                model->labels[i] = "?";
                for(uint8_t j = 0; j < count; j++) {
                    if(default_table[j].index == idx_val) {
                        model->labels[i] = default_table[j].label;
                        break;
                    }
                }
            }

            // Re-resolve the divider position in the new item list
            model->divider_pos = 0xFF;
            if(model->divider_value != 0xFF) {
                for(uint8_t i = 0; i < count; i++) {
                    if(model->indices[i] == model->divider_value) {
                        model->divider_pos = i;
                        break;
                    }
                }
            }

            model->cursor = 0;
        },
        false);
}

void hid_remote_menu_set_selected_index(HidRemoteMenu* menu, uint8_t index_value) {
    furi_assert(menu);
    with_view_model(
        menu->view,
        HidRemoteMenuModel * model,
        {
            for(uint8_t i = 0; i < model->count; i++) {
                if(model->indices[i] == index_value) {
                    // Only place cursor here if the item is in the visible range
                    uint8_t eff = hid_remote_menu_eff_count(model);
                    if(i < eff) {
                        model->cursor = i;
                    }
                    break;
                }
            }
        },
        false);
}

uint8_t hid_remote_menu_get_selected_index(const HidRemoteMenu* menu) {
    furi_assert(menu);
    uint8_t value = 0xFF;
    // Cast away const for with_view_model (only reads model fields).
    with_view_model(
        ((HidRemoteMenu*)menu)->view,
        HidRemoteMenuModel * model,
        {
            if(model->count > 0 && model->cursor < model->count) {
                value = model->indices[model->cursor];
            }
        },
        false);
    return value;
}

void hid_remote_menu_set_select_callback(
    HidRemoteMenu*            menu,
    HidRemoteMenuSelectCallback cb,
    void*                     context) {
    furi_assert(menu);
    menu->select_cb  = cb;
    menu->select_ctx = context;
}

void hid_remote_menu_set_reorder_callback(
    HidRemoteMenu*             menu,
    HidRemoteMenuReorderCallback cb,
    void*                      context) {
    furi_assert(menu);
    menu->reorder_cb  = cb;
    menu->reorder_ctx = context;
}

void hid_remote_menu_set_divider(HidRemoteMenu* menu, uint8_t divider_value) {
    furi_assert(menu);
    with_view_model(
        menu->view,
        HidRemoteMenuModel * model,
        {
            model->divider_value = divider_value;
            model->divider_pos   = 0xFF;

            if(divider_value != 0xFF) {
                for(uint8_t i = 0; i < model->count; i++) {
                    if(model->indices[i] == divider_value) {
                        model->divider_pos = i;
                        break;
                    }
                }
            }

            // Clamp cursor into the new effective visible range
            if(!model->reorder_mode && model->divider_pos != 0xFF && model->count > 0) {
                if(model->cursor > model->divider_pos) {
                    model->cursor = model->divider_pos;
                }
            }
        },
        false);
}
