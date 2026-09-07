/* Janus embedded-C runtime — Stage 4. See architecture.md.
 *
 * Real traversal + tiling + per-kind dispatch + box state — not the
 * original prototype's stub (which drew an empty tile buffer regardless
 * of screen contents). Leaf content used to be a kind-distinct solid fill
 * only; now label/header/button/box-header draw real glyphs over that
 * same fill when a widget has authored `text:` (janus_font.h), and
 * label/header additionally draw a live bound string value when there's
 * no authored `text:` (slice 2 — see janus_read_bound_string below).
 * progress/gauge/checkbox/led remain the pre-existing exception for
 * non-text content — they read the live bound value and vary the fill
 * accordingly. Fill/text colors come from each widget's own `.color`/
 * `.bg_color` (RGB565, authored per-widget in YAML — see
 * emit_embedded_c.py's _pack_rgb565), not a hardcoded runtime constant.
 */
#include "janus_runtime.h"

#include "janus_bound.h"
#include "janus_draw.h"
#include "janus_font.h"
#include "janus_format.h"

#include <stddef.h>
#include <string.h>

/* ---------------------------------------------------------- tile buffer --
 * No malloc anywhere. 16x16x2 bytes (RGB565) = 512 B, well under the
 * ~2 KiB transient-buffer budget (Janus.md) — shrunk from the original
 * mono runtime's 32x32x1 byte tile (1024 B) when the pixel type widened
 * to 16 bits, to keep roughly the same footprint; same tiling loop, just
 * more (smaller) draw_area_sync calls per widget. The blocking traversal
 * (janus_render_screen) drives draw_area_sync directly; the non-blocking
 * path (janus_render_screen_async_start / janus_render_poll, further
 * below) drives draw_area_async/display_busy instead, one call per poll.
 *
 * draw_glyph blits a whole (post-font_scale) glyph in one shot (no
 * tiling), so the buffer also has to hold one full scaled glyph's pixels
 * — at `large`'s native 20x28 that's 560 pixels (1120 B), bigger than the
 * 16x16 fill tile (256 px / 512 B), so it now governs this buffer's size
 * instead. This is exactly why font_scale is capped at large's own
 * footprint (janus_font.h) rather than left open-ended: an uncapped scale
 * would need either an unbounded buffer (blows the ~2 KiB budget) or
 * tiling the glyph blit itself (more moving parts than a v1 with two
 * fixed sizes needs) — capping keeps this single fixed-size buffer
 * sufficient for every combination Stage 1 allows.
 */
#define JANUS_TILE_W 16
#define JANUS_TILE_H 16
#define JANUS_TILE_PIXELS (JANUS_TILE_W * JANUS_TILE_H)
#define JANUS_GLYPH_PIXELS (JANUS_FONT_LARGE_GLYPH_W * JANUS_FONT_LARGE_GLYPH_H)
#define JANUS_TILE_BUFFER_PIXELS \
    (JANUS_GLYPH_PIXELS > JANUS_TILE_PIXELS ? JANUS_GLYPH_PIXELS : JANUS_TILE_PIXELS)
static uint16_t g_tile_buffer[JANUS_TILE_BUFFER_PIXELS];

/* --------------------------------------------------- non-blocking render --
 * janus_render_screen_async_start builds this queue by running the exact
 * same traversal/dispatch as the blocking janus_render_screen (every
 * draw_<kind> function, unchanged) with g_async_enqueue set — fill_rect
 * and draw_glyph below, the only two places that ever call the driver,
 * check the flag and append an op instead of drawing when it's set. That
 * keeps every per-kind draw function, and the geometry/tile-splitting math
 * in fill_rect/draw_string, shared between both render paths — nothing
 * about "how to draw a progress bar" is duplicated for the async case.
 *
 * Fixed capacity (256, matching MOCK_DRIVER_LOG_CAPACITY's existing
 * precedent): a real screen's worth of tile fills + glyphs comfortably
 * fits; a screen that doesn't is a real v1 limit (silently truncated, one
 * `janus_async_queue_overflowed()` check away from being observable) —
 * same "table full, caller falls back" spirit as JANUS_MAX_BOXES.
 *
 * This is a queue built once, not a resumable traversal, deliberately:
 * making fill_rect's tile loop and draw_string's glyph loop themselves
 * suspendable (so a poll could resume mid-loop without a full queue) would
 * need real coroutine/continuation machinery this runtime doesn't have.
 * Building the queue is pure CPU work (no driver calls, so nothing to
 * block on) — only *draining* it, one driver call per janus_render_poll(),
 * needs to be incremental, and a flat array drains one index at a time
 * with no stack or continuation needed. Trade-off: the queue reflects
 * bound values as of janus_render_screen_async_start, not whatever they
 * become while draining — same "snapshot, not live" property any queued
 * frame has.
 *
 * The whole path here is behind JANUS_RENDER_NONBLOCKING (see
 * janus_runtime.h): a `render_mode: blocking` project links none of it,
 * so `g_async_ops` (6912 B) costs it 0 bytes of .bss — the reason this
 * guard exists (channel_icons task 3). The `#else` gives the one
 * un-guarded reader below (`fill_rect`'s tile-buffer prefill) a constant
 * to test, so it always takes the draw path. */
#if defined(JANUS_RENDER_NONBLOCKING)
#define JANUS_MAX_ASYNC_OPS 256
typedef enum {
    JANUS_ASYNC_OP_FILL, JANUS_ASYNC_OP_GLYPH, JANUS_ASYNC_OP_IMAGE
} janus_async_op_kind_t;
typedef struct {
    janus_async_op_kind_t kind;
    int16_t x, y, w, h;            /* GLYPH: w/h unused — recomputed at drain time from
                                     * font_size/scale (font_metrics()), same as draw_glyph itself.
                                     * IMAGE: w/h are this tile's size (<= JANUS_TILE_W/H). */
    uint16_t color;                /* FILL: the fill value. GLYPH: fg */
    uint16_t bg;                   /* GLYPH only */
    const uint8_t *glyph;          /* GLYPH only */
    janus_font_size_t font_size;   /* GLYPH only */
    uint8_t scale;                 /* GLYPH only */
    uint16_t img_slot;             /* IMAGE only: 1-based index into g_image_far, resolved at drain
                                     * time (kept as the small slot, not the 4-byte far address, so
                                     * this op struct — x256 — doesn't grow on AVR) */
    int16_t img_stride;            /* IMAGE only: full image width, to step rows in the image */
    int16_t img_sx, img_sy;        /* IMAGE only: this tile's top-left offset within the image */
} janus_async_op_t;

static janus_async_op_t g_async_ops[JANUS_MAX_ASYNC_OPS];
static uint16_t g_async_op_count = 0;
static uint16_t g_async_cursor = 0;
static bool g_async_enqueue = false;

static void async_enqueue_fill(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t value) {
    if (g_async_op_count >= JANUS_MAX_ASYNC_OPS) return;
    janus_async_op_t *op = &g_async_ops[g_async_op_count++];
    op->kind = JANUS_ASYNC_OP_FILL;
    op->x = x; op->y = y; op->w = w; op->h = h;
    op->color = value;
}

static void async_enqueue_glyph(int16_t x, int16_t y, const uint8_t *glyph,
                                 janus_font_size_t font_size, uint8_t scale,
                                 uint16_t fg, uint16_t bg) {
    if (g_async_op_count >= JANUS_MAX_ASYNC_OPS) return;
    janus_async_op_t *op = &g_async_ops[g_async_op_count++];
    op->kind = JANUS_ASYNC_OP_GLYPH;
    op->x = x; op->y = y;
    op->color = fg; op->bg = bg;
    op->glyph = glyph;
    op->font_size = font_size;
    op->scale = scale;
}

static void async_enqueue_image(int16_t x, int16_t y, int16_t w, int16_t h,
                                 uint16_t img_slot, int16_t stride,
                                 int16_t sx, int16_t sy) {
    if (g_async_op_count >= JANUS_MAX_ASYNC_OPS) return;
    janus_async_op_t *op = &g_async_ops[g_async_op_count++];
    op->kind = JANUS_ASYNC_OP_IMAGE;
    op->x = x; op->y = y; op->w = w; op->h = h;
    op->img_slot = img_slot;
    op->img_stride = stride;
    op->img_sx = sx; op->img_sy = sy;
}
#else
#define g_async_enqueue 0
#endif  /* JANUS_RENDER_NONBLOCKING */

static void fill_rect(janus_rect_t rect, uint16_t value) {
    if (rect.w <= 0 || rect.h <= 0) return;
    if (!g_async_enqueue) {
        for (size_t i = 0; i < JANUS_TILE_W * JANUS_TILE_H; i++) g_tile_buffer[i] = value;
    }

    for (int16_t ty = 0; ty < rect.h; ty += JANUS_TILE_H) {
        int16_t th = (int16_t)(rect.h - ty);
        if (th > JANUS_TILE_H) th = JANUS_TILE_H;
        for (int16_t tx = 0; tx < rect.w; tx += JANUS_TILE_W) {
            int16_t tw = (int16_t)(rect.w - tx);
            if (tw > JANUS_TILE_W) tw = JANUS_TILE_W;
#if defined(JANUS_RENDER_NONBLOCKING)
            if (g_async_enqueue) {
                async_enqueue_fill((int16_t)(rect.x + tx), (int16_t)(rect.y + ty), tw, th, value);
                continue;
            }
#endif
            draw_area_sync((uint16_t)(rect.x + tx), (uint16_t)(rect.y + ty),
                            (uint16_t)tw, (uint16_t)th, g_tile_buffer);
        }
    }
}

/* Splits `rect` into a filled left portion and an empty right portion by
 * `fraction` (clamped to [0, 1]) — used by progress/gauge. */
static void fill_rect_fraction(janus_rect_t rect, double fraction,
                                uint16_t fill_value, uint16_t empty_value) {
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;
    int16_t filled_w = (int16_t)((double)rect.w * fraction);

    janus_rect_t filled = { rect.x, rect.y, filled_w, rect.h };
    janus_rect_t empty = {
        (int16_t)(rect.x + filled_w), rect.y, (int16_t)(rect.w - filled_w), rect.h
    };
    fill_rect(filled, fill_value);
    fill_rect(empty, empty_value);
}

/* ------------------------------------------------------ shape primitives --
 * janus_draw.h's helpers — see that header for the contract. Everything
 * here decomposes to `fill_rect`, so the JANUS_RENDER_NONBLOCKING path
 * captures them as ordinary JANUS_ASYNC_OP_FILL spans with no extra code.
 *
 * `isqrt32` is a plain bit-by-bit integer square root (no <math.h>): for
 * the radii these primitives see (a widget dimension / 2, so well under
 * 256) the operand `r*r - dy*dy` fits comfortably in int32. */
static int16_t isqrt32(int32_t v) {
    if (v <= 0) return 0;
    int32_t rem = v, root = 0, bit = 1L << 30;
    while (bit > v) bit >>= 2;
    while (bit != 0) {
        if (rem >= root + bit) {
            rem -= root + bit;
            root = (root >> 1) + bit;
        } else {
            root >>= 1;
        }
        bit >>= 2;
    }
    return (int16_t)root;
}

/* One horizontal run of `colour`, left-clipped to x >= 0 and dropped if
 * its row is above the panel. `fill_rect` itself doesn't clip negative
 * origins (widget geometry is always on-screen), but a circle/rounded
 * corner can round past an edge, so the shape helpers clip here. */
static void fill_hspan(int16_t x, int16_t y, int16_t w, uint16_t colour) {
    if (y < 0 || w <= 0) return;
    if (x < 0) { w = (int16_t)(w + x); x = 0; }
    if (w <= 0) return;
    janus_rect_t span = { x, y, w, 1 };
    fill_rect(span, colour);
}

void janus_fill_rounded_rect(janus_rect_t rect, int16_t radius, uint16_t colour) {
    if (rect.w <= 0 || rect.h <= 0) return;
    int16_t max_r = (int16_t)((rect.w < rect.h ? rect.w : rect.h) / 2);
    if (radius > max_r) radius = max_r;
    if (radius <= 0) { fill_rect(rect, colour); return; }

    /* centre band: full width, the rows the corners don't touch */
    janus_rect_t band = {
        rect.x, (int16_t)(rect.y + radius), rect.w, (int16_t)(rect.h - 2 * radius)
    };
    fill_rect(band, colour);

    /* `radius` rounded rows, mirrored top and bottom — 2*radius spans,
     * plus the band above == 2*radius + 1 fill_rect calls total */
    for (int16_t dy = 1; dy <= radius; dy++) {
        int16_t inset = (int16_t)(radius - isqrt32((int32_t)radius * radius - (int32_t)dy * dy));
        int16_t span_w = (int16_t)(rect.w - 2 * inset);
        int16_t x = (int16_t)(rect.x + inset);
        fill_hspan(x, (int16_t)(rect.y + radius - dy), span_w, colour);
        fill_hspan(x, (int16_t)(rect.y + rect.h - radius - 1 + dy), span_w, colour);
    }
}

void janus_fill_circle(int16_t cx, int16_t cy, int16_t r, uint16_t colour) {
    if (r <= 0) return;
    for (int16_t dy = (int16_t)(-r); dy <= r; dy++) {
        int16_t half = isqrt32((int32_t)r * r - (int32_t)dy * dy);
        fill_hspan((int16_t)(cx - half), (int16_t)(cy + dy), (int16_t)(2 * half + 1), colour);
    }
}

/* RGB565 channel-wise linear interpolation: t=0 -> a, t=255 -> b. Integer
 * only — used for the LED's rim/highlight and the progress bar's gloss. */
uint16_t janus_rgb565_lerp(uint16_t a, uint16_t b, uint8_t t) {
    int ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
    int br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
    int r = ar + (br - ar) * (int)t / 255;
    int g = ag + (bg - ag) * (int)t / 255;
    int bl = ab + (bb - ab) * (int)t / 255;
    return (uint16_t)((r << 11) | (g << 5) | bl);
}

/* Vertical two-stop gradient: row 0 == `top`, last row == `bottom`, each
 * an interpolated `fill_rect` span (so async-safe). h <= 1 is a flat
 * `top` fill. */
void janus_shade_rect_v(janus_rect_t rect, uint16_t top, uint16_t bottom) {
    if (rect.w <= 0 || rect.h <= 0) return;
    if (rect.h <= 1) { fill_rect(rect, top); return; }
    for (int16_t i = 0; i < rect.h; i++) {
        uint8_t t = (uint8_t)((int32_t)i * 255 / (rect.h - 1));
        janus_rect_t row = { rect.x, (int16_t)(rect.y + i), rect.w, 1 };
        fill_rect(row, janus_rgb565_lerp(top, bottom, t));
    }
}

/* Integer Bresenham; each pixel is a 1x1 span through fill_hspan (so it
 * left-clips x<0 / drops y<0 and stays async-safe). Pixel-at-a-time is
 * deliberate — the only callers are VU scale ticks + one needle. */
void janus_draw_line(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t colour) {
    int16_t dx = (int16_t)(x1 - x0); if (dx < 0) dx = (int16_t)(-dx);
    int16_t dy = (int16_t)(y1 - y0); if (dy < 0) dy = (int16_t)(-dy);
    int16_t sx = x0 < x1 ? 1 : -1;
    int16_t sy = y0 < y1 ? 1 : -1;
    int16_t err = (int16_t)(dx - dy);
    for (;;) {
        fill_hspan(x0, y0, 1, colour);
        if (x0 == x1 && y0 == y1) break;
        int16_t e2 = (int16_t)(2 * err);
        if (e2 > -dy) { err = (int16_t)(err - dy); x0 = (int16_t)(x0 + sx); }
        if (e2 <  dx) { err = (int16_t)(err + dx); y0 = (int16_t)(y0 + sy); }
    }
}

/* ---------------------------------------------------------- fixed-point sin --
 * Quarter-wave Q15 table (sin(deg) * 32767, deg 0..90), flash-resident.
 * janus_sin16/janus_cos16 fold the other three quadrants onto it. Used by
 * the VU needle (vu_meter task 2); no libm, no runtime trig. */
static const int16_t janus_sin_q15[91] JANUS_PROGMEM = {
    0, 572, 1144, 1715, 2286, 2856, 3425, 3993, 4560, 5126,
    5690, 6252, 6813, 7371, 7927, 8481, 9032, 9580, 10126, 10668,
    11207, 11743, 12275, 12803, 13328, 13848, 14364, 14876, 15383, 15886,
    16383, 16876, 17364, 17846, 18323, 18794, 19260, 19720, 20173, 20621,
    21062, 21497, 21925, 22347, 22762, 23170, 23571, 23964, 24351, 24730,
    25101, 25465, 25821, 26169, 26509, 26841, 27165, 27481, 27788, 28087,
    28377, 28659, 28932, 29196, 29451, 29697, 29934, 30162, 30381, 30591,
    30791, 30982, 31163, 31335, 31498, 31650, 31794, 31927, 32051, 32165,
    32269, 32364, 32448, 32523, 32587, 32642, 32687, 32722, 32747, 32762,
    32767,
};

static int16_t sin_q15_at(int idx) {
    return (int16_t)JANUS_PGM_READ_U16(&janus_sin_q15[idx]);
}

int16_t janus_sin16(int16_t deg) {
    int d = deg % 360;
    if (d < 0) d += 360;
    if (d <= 90)  return sin_q15_at(d);
    if (d <= 180) return sin_q15_at(180 - d);
    if (d <= 270) return (int16_t)(-sin_q15_at(d - 180));
    return (int16_t)(-sin_q15_at(360 - d));
}

int16_t janus_cos16(int16_t deg) {
    return janus_sin16((int16_t)(deg + 90));
}

/* --------------------------------------------------------- font tables --
 * Maps a widget's `.font_size` to the table it reads from and that
 * table's native (unscaled) dimensions — the one place that knowledge
 * lives, so draw_string/draw_glyph and the async drain path (further
 * below) never duplicate a `medium` vs `large` branch. */
typedef struct { int16_t w, h; uint8_t col_bytes; } janus_font_metrics_t;

static janus_font_metrics_t font_metrics(janus_font_size_t font_size) {
    if (font_size == JANUS_FONT_SIZE_MEDIUM) {
        janus_font_metrics_t m = {
            JANUS_FONT_MEDIUM_GLYPH_W, JANUS_FONT_MEDIUM_GLYPH_H, JANUS_FONT_MEDIUM_GLYPH_COL_BYTES
        };
        return m;
    }
    janus_font_metrics_t m = {
        JANUS_FONT_LARGE_GLYPH_W, JANUS_FONT_LARGE_GLYPH_H, JANUS_FONT_LARGE_GLYPH_COL_BYTES
    };
    return m;
}

static const uint8_t *font_glyph(janus_font_size_t font_size, char c) {
    return (font_size == JANUS_FONT_SIZE_MEDIUM) ? janus_font_glyph_medium(c) : janus_font_glyph_large(c);
}

/* -------------------------------------------------------------- glyphs --
 * Reuses g_tile_buffer for the scaled glyph's pixels (that's what sizes
 * JANUS_TILE_BUFFER_PIXELS above — same "one shared static scratch
 * buffer, no malloc" discipline as fill_rect above — not a second
 * buffer); draw_string has already clamped `scale` so this never exceeds
 * that buffer (janus_font.h's doc comment on the cap). Each glyph column
 * is `col_bytes` bytes (rows packed 8-per-byte, row 0 = top) since a
 * single byte can't hold every row at either size this runtime ships.
 */
static void draw_glyph(int16_t x, int16_t y, const uint8_t *glyph,
                        janus_font_size_t font_size, uint8_t scale,
                        uint16_t fg, uint16_t bg) {
#if defined(JANUS_RENDER_NONBLOCKING)
    if (g_async_enqueue) {
        async_enqueue_glyph(x, y, glyph, font_size, scale, fg, bg);
        return;
    }
#endif
    janus_font_metrics_t m = font_metrics(font_size);
    int16_t scaled_w = (int16_t)(m.w * scale);
    int16_t scaled_h = (int16_t)(m.h * scale);
    for (int16_t row = 0; row < scaled_h; row++) {
        int16_t src_row = (int16_t)(row / scale);
        for (int16_t col = 0; col < scaled_w; col++) {
            int16_t src_col = (int16_t)(col / scale);
            uint8_t byte = JANUS_PGM_READ_U8(&glyph[src_col * m.col_bytes + src_row / 8]);
            g_tile_buffer[row * scaled_w + col] =
                (byte & (1 << (src_row % 8))) ? fg : bg;
        }
    }
    draw_area_sync((uint16_t)x, (uint16_t)y, (uint16_t)scaled_w, (uint16_t)scaled_h, g_tile_buffer);
}

/* Draws `text` left-aligned, vertically centered in `rect`, in `fg` over a
 * `bg` that must match whatever solid fill the caller already painted
 * `rect` with (unlit glyph pixels reuse it, so the glyph blends into that
 * backdrop instead of punching a mismatched hole in it). No-op if `text`
 * is NULL (unbound widgets keep rendering as a plain solid fill).
 * Clips, never wraps or shrinks the font, once a character would run
 * past `rect`'s right edge — Janus never auto-sizes text at generation
 * time (Janus.md's deferred auto-sizing note), so overflow here is a
 * real, expected v1 case, not a bug to fix in this runtime.
 *
 * `from_flash` distinguishes the two possible sources of `text`: a
 * widget's authored `static_text` (generated, JANUS_PROGMEM on AVR — pass
 * true) vs. a live bound string (janus_read_bound_string, the vendor's own
 * mutable RAM struct — pass false). Reading a flash string with a plain
 * `*p` (or vice versa) is wrong on classic AVR, so callers must get this
 * right; off-AVR both paths behave identically either way.
 *
 * `font_size`/`scale` come straight from the widget desc. `scale` is
 * clamped here (not trusted from the caller) to whatever keeps the
 * scaled glyph within g_tile_buffer's fixed size — Stage 1 (dsl_yaml.py)
 * already rejects an authored combination that would need clamping, but
 * this runtime doesn't get to assume every widget desc it's ever handed
 * went through that validation (hand-built test fixtures, a zero-
 * initialized `.font_scale` — see janus_font.h), so it protects its own
 * buffer regardless. */
static void draw_string(janus_rect_t rect, const char *text, uint16_t fg, uint16_t bg, bool from_flash,
                         janus_font_size_t font_size, uint8_t scale) {
    if (text == NULL) return;
    if (scale < 1) scale = 1;

    janus_font_metrics_t m = font_metrics(font_size);
    while (scale > 1 && (int32_t)(m.w * scale) * (m.h * scale) > JANUS_TILE_BUFFER_PIXELS) scale--;

    int16_t glyph_w = (int16_t)(m.w * scale);
    int16_t glyph_h = (int16_t)(m.h * scale);

    int16_t y = (int16_t)(rect.y + (rect.h - glyph_h) / 2);
    if (y < rect.y) y = rect.y;
    int16_t x = (int16_t)(rect.x + 1);
    int16_t right = (int16_t)(rect.x + rect.w);

    for (const char *p = text; ; p++) {
        char c = from_flash ? (char)JANUS_PGM_READ_U8(p) : *p;
        if (c == '\0') break;
        if ((int16_t)(x + glyph_w) > right) break;
        const uint8_t *glyph = font_glyph(font_size, c);
        if (glyph != NULL) draw_glyph(x, y, glyph, font_size, scale, fg, bg);
        x = (int16_t)(x + glyph_w + scale);
    }
}

/* ---------------------------------------------------------- focus ring --
 * Stage 6: the visual marker for "this is the currently focused widget"
 * (encoder/button navigation — touch never sets this). A thin outline
 * drawn over whatever the widget's own draw_<kind>() already painted,
 * reusing fill_rect/g_tile_buffer, no new buffer. Fixed runtime color,
 * deliberately not authorable per-widget — it's a Janus-owned UI
 * affordance, not widget content.
 */
#define JANUS_COLOR_FOCUS_RING ((uint16_t)0x07ff)  /* cyan */

/* Only button and box are ever focusable (Stage 3b's _assign_focus_order
 * — everything else keeps JANUS_FOCUS_NONE), so this is the one piece of
 * mutable focus state the whole module needs; draw_button/draw_box_header
 * below just compare their own pointer against it. */
static const janus_widget_desc_t *g_focused_widget = NULL;

static void draw_focus_ring(janus_rect_t r) {
    janus_rect_t top    = { r.x, r.y, r.w, 1 };
    janus_rect_t bottom = { r.x, (int16_t)(r.y + r.h - 1), r.w, 1 };
    janus_rect_t left   = { r.x, r.y, 1, r.h };
    janus_rect_t right  = { (int16_t)(r.x + r.w - 1), r.y, 1, r.h };
    fill_rect(top, JANUS_COLOR_FOCUS_RING);
    fill_rect(bottom, JANUS_COLOR_FOCUS_RING);
    fill_rect(left, JANUS_COLOR_FOCUS_RING);
    fill_rect(right, JANUS_COLOR_FOCUS_RING);
}

/* ------------------------------------------------------------ box state --
 * janus_widget_desc_t instances are static const arrays baked at
 * generation time — nowhere in them to hold a *mutable* expand/collapse
 * bit. This small fixed-capacity table holds it instead, keyed by
 * descriptor pointer identity (stable for the program's lifetime).
 */
#define JANUS_MAX_BOXES 16
typedef struct {
    const janus_widget_desc_t *box;
    bool expanded;
} janus_box_state_t;
static janus_box_state_t g_box_state[JANUS_MAX_BOXES];
static uint8_t g_box_state_count = 0;

static janus_box_state_t *box_state_find_or_register(const janus_widget_desc_t *box) {
    for (uint8_t i = 0; i < g_box_state_count; i++) {
        if (g_box_state[i].box == box) return &g_box_state[i];
    }
    if (g_box_state_count < JANUS_MAX_BOXES) {
        janus_box_state_t *slot = &g_box_state[g_box_state_count++];
        slot->box = box;
        slot->expanded = janus_widget_load(box).initial_expanded;
        return slot;
    }
    return NULL; /* table full: caller falls back to initial_expanded, never toggles */
}

bool janus_box_is_expanded(const janus_widget_desc_t *box) {
    janus_box_state_t *slot = box_state_find_or_register(box);
    if (slot != NULL) return slot->expanded;
    return janus_widget_load(box).initial_expanded;
}

/* Only one screen's widgets are ever live at once (Janus.md) — this is
 * how janus_toggle_box, whose spec'd signature takes only the box
 * pointer, still finds the right bound_struct for re-rendering any
 * bound children inside it. */
static const janus_screen_desc_t *g_current_screen = NULL;
/* The current screen's resolved image far-address table (janus_screen_desc_t
 * .image_far, populated by its .resolve_images). Set on every screen-enter
 * alongside g_current_screen; a widget's image_slot is a 1-based index
 * into it. NULL for a screen that bakes no images. */
static const janus_farptr_t *g_image_far = NULL;

/* --------------------------------------------------------- bound reads --
 * janus_read_bound_value / janus_read_bound_string live in janus_bound.c
 * now (label_format task 1) — split out so janus_format.c can share the
 * offsetof-into-bound_struct math without linking this whole module.
 * Both behave exactly as the former file-static janus_read_bound_value /
 * janus_read_bound_string did. */

/* ------------------------------------------------- per-kind draw_<kind> --
 * Every widget's fill/text color comes from its own `.color` (ink /
 * foreground / on-state) and `.bg_color` (background / off-state) —
 * authored per-widget in YAML, packed to RGB565 at generation time
 * (emit_embedded_c.py's _pack_rgb565), defaulted by Stage 3b to
 * JANUS_COLOR_DEFAULT_FG/_BG when omitted. Never a runtime constant.
 */

/* label/header text resolution, in priority order:
 *   1. `text_is_format` -> `static_text` is a printf-style template;
 *      format it against `bind` into a stack buffer (janus_format_into)
 *      and draw that (RAM, so from_flash = false).
 *   2. plain authored `static_text` -> draw it verbatim from flash.
 *      (A widget authored with both `text:` and `bind:` but no format
 *      conversion lands here — static_text is the deterministic
 *      tie-break, Janus.md's catalog documents them as one-or-the-other.)
 *   3. otherwise the live bound string, if any. */
static void draw_text_leaf(const janus_widget_desc_t *w, const void *bound_struct) {
    janus_widget_desc_t lw = janus_widget_load(w);
    fill_rect(lw.geometry, lw.bg_color);

    if (lw.text_is_format && lw.static_text != NULL) {
        char buf[JANUS_FORMAT_BUF];
        janus_format_into(buf, sizeof buf, lw.static_text, &lw.bind, bound_struct);
        draw_string(lw.geometry, buf, lw.color, lw.bg_color, false, lw.font_size, lw.font_scale);
        return;
    }

    bool from_flash = lw.static_text != NULL;
    const char *text = from_flash ? lw.static_text : janus_read_bound_string(&lw.bind, bound_struct);
    draw_string(lw.geometry, text, lw.color, lw.bg_color, from_flash, lw.font_size, lw.font_scale);
}
static void draw_label(const janus_widget_desc_t *w, const void *bound_struct) {
    draw_text_leaf(w, bound_struct);
}
static void draw_header(const janus_widget_desc_t *w, const void *bound_struct) {
    draw_text_leaf(w, bound_struct);
}
static void draw_button(const janus_widget_desc_t *w) {
    janus_widget_desc_t lw = janus_widget_load(w);
    fill_rect(lw.geometry, lw.bg_color);
    draw_string(lw.geometry, lw.static_text, lw.color, lw.bg_color, true, lw.font_size, lw.font_scale);
    if (w == g_focused_widget) draw_focus_ring(lw.geometry);
}
/* "missing texture" magenta — a `file:` was authored on an image widget
 * but Stage 3b couldn't find/decode it (widget.image_error). Janus-owned,
 * not authorable, same spirit as JANUS_COLOR_FOCUS_RING. */
#define JANUS_COLOR_IMAGE_MISSING ((uint16_t)0xf81f)

/* Blits a pre-decoded RGB565 image (JANUS_PROGMEM `src`, src_w x src_h,
 * row-major) into `rect` at 1:1 — the image was already rescaled to the
 * widget's geometry at generation time, so src_w/src_h normally equal
 * rect.w/rect.h; anything larger is clipped, anything smaller leaves the
 * remainder untouched. Same tile loop / shared g_tile_buffer / async-flag
 * split as fill_rect — a tile is <= JANUS_TILE_W x JANUS_TILE_H, so one
 * tile's worth of pixels always fits g_tile_buffer. Each source row of a
 * tile is one JANUS_MEMCPY_PF — a far copy (memcpy_PF) on AVR so the
 * source can live past the 64 KiB near-flash window, a plain memcpy on
 * host. `image_slot` (1-based) indexes g_image_far for the source's far
 * address; the async path carries the slot, not the address, and
 * re-resolves at drain (keeps the op struct small).
 */
static void blit_image(janus_rect_t rect, uint16_t image_slot,
                        int16_t src_w, int16_t src_h) {
    int16_t draw_w = rect.w < src_w ? rect.w : src_w;
    int16_t draw_h = rect.h < src_h ? rect.h : src_h;
    if (draw_w <= 0 || draw_h <= 0 || image_slot == 0 || g_image_far == NULL) return;
    janus_farptr_t src = g_image_far[image_slot - 1];

    for (int16_t ty = 0; ty < draw_h; ty += JANUS_TILE_H) {
        int16_t th = (int16_t)(draw_h - ty);
        if (th > JANUS_TILE_H) th = JANUS_TILE_H;
        for (int16_t tx = 0; tx < draw_w; tx += JANUS_TILE_W) {
            int16_t tw = (int16_t)(draw_w - tx);
            if (tw > JANUS_TILE_W) tw = JANUS_TILE_W;
#if defined(JANUS_RENDER_NONBLOCKING)
            if (g_async_enqueue) {
                async_enqueue_image((int16_t)(rect.x + tx), (int16_t)(rect.y + ty),
                                    tw, th, image_slot, src_w, tx, ty);
                continue;
            }
#endif
            for (int16_t row = 0; row < th; row++) {
                JANUS_MEMCPY_PF(&g_tile_buffer[row * tw],
                                JANUS_FAR_ADD(src, ((size_t)(ty + row) * src_w + tx)
                                                       * sizeof(uint16_t)),
                                (size_t)tw * sizeof(uint16_t));
            }
            draw_area_sync((uint16_t)(rect.x + tx), (uint16_t)(rect.y + ty),
                            (uint16_t)tw, (uint16_t)th, g_tile_buffer);
        }
    }
}

static void draw_image(const janus_widget_desc_t *w) {
    janus_widget_desc_t lw = janus_widget_load(w);
    if (lw.image_error) {
        fill_rect(lw.geometry, JANUS_COLOR_IMAGE_MISSING);
        return;
    }
    if (lw.image_slot == 0 || g_image_far == NULL) {
        /* no `file:` authored (pre-image v1 stub — a `color` fill), or an
         * image widget refreshed on its own before any full screen render
         * populated g_image_far for its screen. */
        fill_rect(lw.geometry, lw.color);
        return;
    }
    blit_image(lw.geometry, lw.image_slot, (int16_t)lw.image_w, (int16_t)lw.image_h);
}
static void draw_radiobutton(const janus_widget_desc_t *w) {
    janus_widget_desc_t lw = janus_widget_load(w);
    fill_rect(lw.geometry, lw.color);
}
static void draw_divider(const janus_widget_desc_t *w) {
    janus_widget_desc_t lw = janus_widget_load(w);
    fill_rect(lw.geometry, lw.color);
}

/* progress/gauge share one render: a recessed rounded track (.bg_color),
 * a rounded proportional fill (.color), and a 1px-ish gloss along the top
 * of the filled part. Both kinds land here — a distinct radial gauge is a
 * later increment (ui_widgets/kind_visuals task 2). */
static void draw_progress_or_gauge(const janus_widget_desc_t *w, const void *bound_struct) {
    janus_widget_desc_t lw = janus_widget_load(w);
    double value = janus_read_bound_value(&lw.bind, bound_struct);
    double span = (double)lw.bind.range_max - (double)lw.bind.range_min;
    double fraction = span != 0.0 ? (value - lw.bind.range_min) / span : 0.0;
    if (fraction < 0.0) fraction = 0.0;
    if (fraction > 1.0) fraction = 1.0;

    janus_rect_t r = lw.geometry;
    int16_t radius = (int16_t)(r.h / 2);
    janus_fill_rounded_rect(r, radius, lw.bg_color);

    int16_t filled_w = (int16_t)((double)r.w * fraction);
    if (filled_w > 0) {
        janus_rect_t fill = { r.x, r.y, filled_w, r.h };
        janus_fill_rounded_rect(fill, radius, lw.color);
        int16_t gloss_h = (int16_t)(r.h / 3);
        if (gloss_h < 1) gloss_h = 1;
        janus_rect_t gloss = { r.x, r.y, filled_w, gloss_h };
        janus_shade_rect_v(gloss, janus_rgb565_lerp(lw.color, 0xffff, 64), lw.color);
    }
}

static void draw_checkbox(const janus_widget_desc_t *w, const void *bound_struct) {
    janus_widget_desc_t lw = janus_widget_load(w);
    double value = janus_read_bound_value(&lw.bind, bound_struct);
    fill_rect(lw.geometry, value != 0.0 ? lw.color : lw.bg_color);
}

/* led: a round, shaded indicator — a darker rim ring, the state-colour
 * face on top, and a small lighter specular highlight up-and-left. State
 * -> colour selection is unchanged (0 -> .bg_color, 1 -> .color, >=2 ->
 * amber); this only changes how that colour is painted. Rim/highlight
 * are derived from the state colour via janus_rgb565_lerp, so there's
 * nothing new to author (ui_widgets/kind_visuals task 3). */
static void draw_led(const janus_widget_desc_t *w, const void *bound_struct) {
    janus_widget_desc_t lw = janus_widget_load(w);
    int state = (int)janus_read_bound_value(&lw.bind, bound_struct);
    uint16_t colour = state <= 0 ? lw.bg_color : (state == 1 ? lw.color : JANUS_COLOR_LED_WARN);

    janus_rect_t r = lw.geometry;
    int16_t cx = (int16_t)(r.x + r.w / 2);
    int16_t cy = (int16_t)(r.y + r.h / 2);
    int16_t rad = (int16_t)((r.w < r.h ? r.w : r.h) / 2);
    if (rad <= 0) { fill_rect(r, colour); return; }

    janus_fill_circle(cx, cy, rad, janus_rgb565_lerp(colour, 0x0000, 80));       /* rim */
    janus_fill_circle(cx, cy, (int16_t)(rad - 1), colour);                        /* face */
    int16_t hl_r = (int16_t)(rad / 3);
    if (hl_r < 1) hl_r = 1;
    janus_fill_circle((int16_t)(cx - rad / 3), (int16_t)(cy - rad / 3), hl_r,
                      janus_rgb565_lerp(colour, 0xffff, 130));                    /* highlight */
}

/* badge/slider still reuse checkbox's / progress's bind logic as a plain
 * fill — toggle is where that stops: it renders a switch (rounded pill
 * track + a circular knob that sits left when off, right when on), so it
 * no longer looks like a checkbox. Knob colour is a lightened copy of
 * whichever track colour is showing, so it reads in both states with no
 * new authored field (ui_widgets/kind_visuals task 1). */
static void draw_toggle(const janus_widget_desc_t *w, const void *bound_struct) {
    janus_widget_desc_t lw = janus_widget_load(w);
    bool on = janus_read_bound_value(&lw.bind, bound_struct) != 0.0;
    janus_rect_t r = lw.geometry;

    int16_t half_w = (int16_t)(r.w / 2);
    int16_t kd = r.h < half_w ? r.h : half_w;          /* knob diameter */
    int16_t pad = 1;
    uint16_t track = on ? lw.color : lw.bg_color;

    janus_fill_rounded_rect(r, (int16_t)(r.h / 2), track);
    int16_t cx = on ? (int16_t)(r.x + r.w - kd / 2 - pad)
                    : (int16_t)(r.x + kd / 2 + pad);
    janus_fill_circle(cx, (int16_t)(r.y + r.h / 2), (int16_t)(kd / 2 - pad),
                      janus_rgb565_lerp(track, 0xffff, 96));
}

static void draw_badge(const janus_widget_desc_t *w, const void *bound_struct) {
    janus_widget_desc_t lw = janus_widget_load(w);
    double value = janus_read_bound_value(&lw.bind, bound_struct);
    fill_rect(lw.geometry, value != 0.0 ? lw.color : lw.bg_color);
}

static void draw_slider(const janus_widget_desc_t *w, const void *bound_struct) {
    janus_widget_desc_t lw = janus_widget_load(w);
    double value = janus_read_bound_value(&lw.bind, bound_struct);
    double span = (double)lw.bind.range_max - (double)lw.bind.range_min;
    double fraction = span != 0.0 ? (value - lw.bind.range_min) / span : 0.0;
    fill_rect_fraction(lw.geometry, fraction, lw.color, lw.bg_color);
}

/* forward declaration: draw_box_header (below) renders `summary_children`
 * via render_widget, and render_widget's JANUS_WIDGET_BOX case calls
 * draw_box_header — genuine mutual recursion, one of the two needs a
 * prototype ahead of its definition.
 *
 * `bound_dirty` (added 2026-09-05, threaded through both): NULL means
 * "force draw regardless" (today's behavior, unchanged — every existing
 * caller passes NULL); a real pointer means "skip a bound leaf whose
 * field's dirty bit isn't set" — see bind_consume_dirty below and
 * janus_render_widget_if_dirty/janus_render_screen_if_dirty. */
static void render_widget(const janus_widget_desc_t *w, const void *bound_struct, void *bound_dirty);

/* Checks (and, if set, clears) whether `bind`'s own field is marked dirty
 * in `bound_dirty` — same offsetof-into-a-generated-struct mechanism
 * janus_read_bound_value already uses for the *value* struct, just a bool
 * instead. Always "yes, draw" for an unbound widget or a NULL
 * bound_dirty (the force-draw case) — nothing to check against, so the
 * safe default is to draw. */
static bool bind_consume_dirty(const janus_bind_t *bind, void *bound_dirty) {
    if (bound_dirty == NULL || bind->field_type == JANUS_FIELD_NONE) return true;
    bool *flag = (bool *)((uint8_t *)bound_dirty + bind->dirty_offset);
    if (!*flag) return false;
    *flag = false;
    return true;
}

/* box's own content is just its header strip; children are separate
 * descriptors, drawn (or not) by the traversal below. Its title text is
 * `box.static_text` — box has no dedicated title field, it reuses the
 * generic Widget.text (Janus.md's widget catalog / architecture.md
 * Stage 2). The header fill/title itself always draws when reached
 * (box has no `bind` of its own to check dirty against) — only the
 * individual summary_children below are dirty-checked. */
static void draw_box_header(const janus_widget_desc_t *box, const void *bound_struct, void *bound_dirty) {
    janus_widget_desc_t lb = janus_widget_load(box);
    fill_rect(lb.geometry_collapsed, lb.bg_color);
    draw_string(lb.geometry_collapsed, lb.static_text, lb.color, lb.bg_color, true, lb.font_size, lb.font_scale);
    /* summary widgets always render here, collapsed or expanded — unlike
     * lb.children, which only render when the box is actually expanded
     * (see the JANUS_WIDGET_BOX case below / janus_toggle_box). */
    for (uint16_t i = 0; i < lb.summary_child_count; i++) {
        render_widget(&lb.summary_children[i], bound_struct, bound_dirty);
    }
    if (box == g_focused_widget) draw_focus_ring(lb.geometry_collapsed);
}

/* ---------------------------------------------------------- traversal --
 */
static void render_widget(const janus_widget_desc_t *w, const void *bound_struct, void *bound_dirty) {
    janus_widget_desc_t lw = janus_widget_load(w);

    /* every leaf kind below draws from *live* bound data (or none at
     * all) — this one check covers all of them, same rule regardless of
     * kind: unbound or dirty -> draw (and clear the bit); clean -> skip. */
    switch (lw.kind) {
        case JANUS_WIDGET_LABEL: case JANUS_WIDGET_HEADER: case JANUS_WIDGET_BUTTON:
        case JANUS_WIDGET_IMAGE: case JANUS_WIDGET_RADIOBUTTON: case JANUS_WIDGET_PROGRESS:
        case JANUS_WIDGET_GAUGE: case JANUS_WIDGET_CHECKBOX: case JANUS_WIDGET_LED:
        case JANUS_WIDGET_DIVIDER: case JANUS_WIDGET_TOGGLE: case JANUS_WIDGET_BADGE:
        case JANUS_WIDGET_SLIDER:
            if (!bind_consume_dirty(&lw.bind, bound_dirty)) return;
            break;
        default:
            break;
    }

    switch (lw.kind) {
        case JANUS_WIDGET_LABEL: draw_label(w, bound_struct); return;
        case JANUS_WIDGET_HEADER: draw_header(w, bound_struct); return;
        case JANUS_WIDGET_BUTTON: draw_button(w); return;
        case JANUS_WIDGET_IMAGE: draw_image(w); return;
        case JANUS_WIDGET_RADIOBUTTON: draw_radiobutton(w); return;
        case JANUS_WIDGET_PROGRESS:
        case JANUS_WIDGET_GAUGE: draw_progress_or_gauge(w, bound_struct); return;
        case JANUS_WIDGET_CHECKBOX: draw_checkbox(w, bound_struct); return;
        case JANUS_WIDGET_LED: draw_led(w, bound_struct); return;
        case JANUS_WIDGET_DIVIDER: draw_divider(w); return;
        case JANUS_WIDGET_TOGGLE: draw_toggle(w, bound_struct); return;
        case JANUS_WIDGET_BADGE: draw_badge(w, bound_struct); return;
        case JANUS_WIDGET_SLIDER: draw_slider(w, bound_struct); return;

        case JANUS_WIDGET_BOX:
            draw_box_header(w, bound_struct, bound_dirty);
            if (janus_box_is_expanded(w)) {
                for (uint16_t i = 0; i < lw.child_count; i++) {
                    render_widget(&lw.children[i], bound_struct, bound_dirty);
                }
            }
            return;

        /* structural containers — no pixels of their own (Janus.md widget catalog) */
        case JANUS_WIDGET_COLUMN:
        case JANUS_WIDGET_ROW:
        case JANUS_WIDGET_RADIOGROUP:
            for (uint16_t i = 0; i < lw.child_count; i++) {
                render_widget(&lw.children[i], bound_struct, bound_dirty);
            }
            return;
    }
}

const janus_screen_desc_t *janus_app_get_screen(const janus_app_t *app, uint16_t index) {
    if (index >= app->screen_count) return NULL;
    return JANUS_PGM_READ_PTR(&app->screens[index]);
}

void janus_render_widget(const janus_widget_desc_t *widget, const void *bound_struct) {
    render_widget(widget, bound_struct, NULL);
}

void janus_render_widget_if_dirty(const janus_widget_desc_t *widget, const void *bound_struct, void *bound_dirty) {
    render_widget(widget, bound_struct, bound_dirty);
}

/* Every screen-enter routes through one of these two (switch_screen and
 * render_screen_async_start both call janus_render_screen) — the single
 * place to (re)resolve the screen's image far-addresses and point
 * g_image_far at them before any draw_image can run. */
static void enter_screen(const janus_screen_desc_t *screen, janus_screen_desc_t *ls_out) {
    g_current_screen = screen;
    *ls_out = janus_screen_load(screen);
    if (ls_out->resolve_images != NULL) ls_out->resolve_images();
    g_image_far = ls_out->image_far;
}

void janus_render_screen(const janus_screen_desc_t *screen) {
    janus_screen_desc_t ls;
    enter_screen(screen, &ls);
    for (uint16_t i = 0; i < ls.widget_count; i++) {
        render_widget(&ls.widgets[i], ls.bound_struct, NULL);
    }
}

void janus_render_screen_if_dirty(const janus_screen_desc_t *screen) {
    janus_screen_desc_t ls;
    enter_screen(screen, &ls);
    for (uint16_t i = 0; i < ls.widget_count; i++) {
        render_widget(&ls.widgets[i], ls.bound_struct, ls.bound_dirty);
    }
}

void janus_switch_screen(janus_app_t *app, uint16_t screen_index) {
    if (screen_index >= app->screen_count) return;
    /* Clear focus *before* switching — g_focused_widget would otherwise
     * point into the outgoing screen's static widget array; if left set,
     * the next janus_set_focus call would try to redraw that stale
     * widget on top of the freshly rendered new screen. Callers using
     * encoder/button navigation re-establish focus on the new screen
     * with janus_focus_move(new_screen, 0) right after this. */
    janus_set_focus(NULL);
    app->active_screen = screen_index;
    janus_render_screen(janus_app_get_screen(app, screen_index));
}

#if defined(JANUS_RENDER_NONBLOCKING)
void janus_render_screen_async_start(const janus_screen_desc_t *screen) {
    g_async_op_count = 0;
    g_async_cursor = 0;
    g_async_enqueue = true;
    janus_render_screen(screen);   /* every draw_<kind> call now enqueues, not draws */
    g_async_enqueue = false;
}

bool janus_render_poll(void) {
    if (g_async_cursor >= g_async_op_count) return false;
    if (display_busy()) return true;   /* still rendering — try again next poll */

    const janus_async_op_t *op = &g_async_ops[g_async_cursor];
    if (op->kind == JANUS_ASYNC_OP_FILL) {
        for (size_t i = 0; i < JANUS_TILE_W * JANUS_TILE_H; i++) g_tile_buffer[i] = op->color;
        draw_area_async((uint16_t)op->x, (uint16_t)op->y, (uint16_t)op->w, (uint16_t)op->h, g_tile_buffer);
    } else if (op->kind == JANUS_ASYNC_OP_IMAGE) {
        janus_farptr_t src = (g_image_far != NULL && op->img_slot != 0)
                             ? g_image_far[op->img_slot - 1] : (janus_farptr_t)0;
        for (int16_t row = 0; row < op->h; row++) {
            JANUS_MEMCPY_PF(&g_tile_buffer[row * op->w],
                            JANUS_FAR_ADD(src, ((size_t)(op->img_sy + row) * op->img_stride
                                                    + op->img_sx) * sizeof(uint16_t)),
                            (size_t)op->w * sizeof(uint16_t));
        }
        draw_area_async((uint16_t)op->x, (uint16_t)op->y, (uint16_t)op->w, (uint16_t)op->h, g_tile_buffer);
    } else {
        janus_font_metrics_t m = font_metrics(op->font_size);
        int16_t scaled_w = (int16_t)(m.w * op->scale);
        int16_t scaled_h = (int16_t)(m.h * op->scale);
        for (int16_t row = 0; row < scaled_h; row++) {
            int16_t src_row = (int16_t)(row / op->scale);
            for (int16_t col = 0; col < scaled_w; col++) {
                int16_t src_col = (int16_t)(col / op->scale);
                uint8_t byte = JANUS_PGM_READ_U8(&op->glyph[src_col * m.col_bytes + src_row / 8]);
                g_tile_buffer[row * scaled_w + col] =
                    (byte & (1 << (src_row % 8))) ? op->color : op->bg;
            }
        }
        draw_area_async((uint16_t)op->x, (uint16_t)op->y, (uint16_t)scaled_w, (uint16_t)scaled_h, g_tile_buffer);
    }
    g_async_cursor++;
    return g_async_cursor < g_async_op_count;
}

void janus_switch_screen_async_start(janus_app_t *app, uint16_t screen_index) {
    if (screen_index >= app->screen_count) return;
    janus_set_focus(NULL);   /* same reasoning as janus_switch_screen */
    app->active_screen = screen_index;
    janus_render_screen_async_start(janus_app_get_screen(app, screen_index));
}
#endif  /* JANUS_RENDER_NONBLOCKING */

void janus_set_focus(const janus_widget_desc_t *widget) {
    const janus_widget_desc_t *previous = g_focused_widget;
    if (previous == widget) return;

    const void *bound_struct = g_current_screen != NULL ? janus_screen_load(g_current_screen).bound_struct : NULL;
    g_focused_widget = widget;
    if (previous != NULL) render_widget(previous, bound_struct, NULL);
    if (widget != NULL) render_widget(widget, bound_struct, NULL);
}

const janus_widget_desc_t *janus_get_focus(void) {
    return g_focused_widget;
}

void janus_toggle_box(const janus_widget_desc_t *box) {
    janus_box_state_t *slot = box_state_find_or_register(box);
    if (slot != NULL) {
        slot->expanded = !slot->expanded;
    }

    const void *bound_struct = g_current_screen != NULL ? janus_screen_load(g_current_screen).bound_struct : NULL;
    janus_widget_desc_t lb = janus_widget_load(box);
    /* Clear the box's full (expanded-size) footprint before redrawing —
     * draw_box_header only repaints the header strip, and .children only
     * get redrawn when expanded, so collapsing would otherwise leave the
     * previous render's child pixels on screen: nothing else ever repaints
     * a rect a widget doesn't currently own. Found on real ArduinoIHM
     * hardware, ported back 2026-09-05 (previously only a local,
     * never-upstreamed fix — lost the first time this repo's runtime got
     * vendored back over it). */
    fill_rect(lb.geometry, lb.bg_color);
    draw_box_header(box, bound_struct, NULL);
    if (janus_box_is_expanded(box)) {
        for (uint16_t i = 0; i < lb.child_count; i++) {
            render_widget(&lb.children[i], bound_struct, NULL);
        }
    }
}
