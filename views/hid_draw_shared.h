#pragma once

#include <gui/canvas.h>

// Small directional arrow glyphs shared by the remote views. Two variants:
// - dot: triangle with a single dot tip (media/movie/music transport symbols)
// - line: triangle with a stem line through it (keynote d-pad symbols)

static inline void
    hid_draw_arrow_dot(Canvas* canvas, uint8_t x, uint8_t y, CanvasDirection dir) {
    canvas_draw_triangle(canvas, x, y, 5, 3, dir);
    if(dir == CanvasDirectionBottomToTop) {
        canvas_draw_dot(canvas, x, y - 1);
    } else if(dir == CanvasDirectionTopToBottom) {
        canvas_draw_dot(canvas, x, y + 1);
    } else if(dir == CanvasDirectionRightToLeft) {
        canvas_draw_dot(canvas, x - 1, y);
    } else if(dir == CanvasDirectionLeftToRight) {
        canvas_draw_dot(canvas, x + 1, y);
    }
}

static inline void
    hid_draw_arrow_line(Canvas* canvas, uint8_t x, uint8_t y, CanvasDirection dir) {
    canvas_draw_triangle(canvas, x, y, 5, 3, dir);
    if(dir == CanvasDirectionBottomToTop) {
        canvas_draw_line(canvas, x, y + 6, x, y - 1);
    } else if(dir == CanvasDirectionTopToBottom) {
        canvas_draw_line(canvas, x, y - 6, x, y + 1);
    } else if(dir == CanvasDirectionRightToLeft) {
        canvas_draw_line(canvas, x + 6, y, x - 1, y);
    } else if(dir == CanvasDirectionLeftToRight) {
        canvas_draw_line(canvas, x - 6, y, x + 1, y);
    }
}
