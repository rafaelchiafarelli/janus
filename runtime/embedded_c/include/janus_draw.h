/* Janus embedded-C runtime — shape primitives (Janus-internal).
 *
 * Allocation-free helpers for the richer per-kind renders (toggle switch,
 * progress bar, shaded LED, VU needle — the ui_widgets/kind_visuals and
 * vu_meter epics). Every one is built entirely out of `fill_rect`, so
 * the blocking path and the JANUS_RENDER_NONBLOCKING (queued/polled)
 * path both capture them with no new async op kind — a rounded rect or a
 * circle enqueues plain JANUS_ASYNC_OP_FILL spans like any other fill.
 *
 * Defined in janus_runtime.c beside `fill_rect`; declared here so the
 * per-kind draw_<kind> functions and the unit tests can reach them. This
 * is NOT the vendor-facing API (janus_runtime.h) — it is internal to the
 * fixed library.
 */
#ifndef JANUS_DRAW_H
#define JANUS_DRAW_H

#include <stdint.h>

#include "janus_runtime.h"   /* janus_rect_t */

/* Filled rounded rectangle. `radius <= 0` degrades to a plain
 * `fill_rect`; `radius` is clamped to `min(rect.w, rect.h) / 2`. Emits
 * `2*radius + 1` horizontal spans (one centre band + `radius` rows top
 * and bottom), each left-clipped to x >= 0. */
void janus_fill_rounded_rect(janus_rect_t rect, int16_t radius, uint16_t colour);

/* Filled circle centred at `(cx, cy)`, radius `r`. `r <= 0` is a no-op.
 * Emits `2*r + 1` horizontal spans, each left-clipped to x >= 0 and
 * skipped entirely when its row is above y = 0. */
void janus_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t colour);

/* RGB565 channel-wise linear interpolation: `t == 0` -> `a`,
 * `t == 255` -> `b`. Integer only, no allocation. */
uint16_t janus_rgb565_lerp(uint16_t a, uint16_t b, uint8_t t);

/* Vertical two-stop gradient over `rect`: top row == `top`, bottom row
 * == `bottom`, one interpolated `fill_rect` span per row. `rect.h <= 1`
 * degrades to a flat `top` fill. */
void janus_shade_rect_v(janus_rect_t rect, uint16_t top, uint16_t bottom);

/* Integer Bresenham line, one 1x1 span per pixel (left-clipped to x >= 0,
 * rows above y = 0 dropped). Pixel-at-a-time — sized for VU ticks + a
 * single needle, not bulk drawing. */
void janus_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t colour);

/* Fixed-point sine / cosine of an angle in **degrees**, result in Q15
 * (`sin * 32767`, so -32767..32767). Backed by a 0..90 quarter-wave
 * flash table; the other quadrants are folded onto it. No libm.
 * `janus_cos16(d) == janus_sin16(d + 90)`. */
int16_t janus_sin16(int16_t deg);
int16_t janus_cos16(int16_t deg);

#endif /* JANUS_DRAW_H */
