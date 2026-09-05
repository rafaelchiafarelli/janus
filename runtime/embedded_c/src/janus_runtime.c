/* Janus embedded-C runtime — Stage 4. See architecture.md.
 *
 * Real traversal + tiling + per-kind dispatch + box state — not the
 * original prototype's stub (which drew an empty tile buffer regardless
 * of screen contents). Leaf content used to be a kind-distinct solid fill
 * only; now label/header/button/box-header draw real glyphs over that
 * same fill when a widget has authored `text:` (janus_font.h), and
 * label/header additionally draw a live bound string value when there's
 * no authored `text:` (slice 2 — see read_bound_string below).
 * progress/gauge/checkbox/led remain the pre-existing exception for
 * non-text content — they read the live bound value and vary the fill
 * accordingly. Fill/text colors come from each widget's own `.color`/
 * `.bg_color` (RGB565, authored per-widget in YAML — see
 * emit_embedded_c.py's _pack_rgb565), not a hardcoded runtime constant.
 */
#include "janus_runtime.h"

#include "janus_font.h"

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
 */
#define JANUS_TILE_W 16
#define JANUS_TILE_H 16
static uint16_t g_tile_buffer[JANUS_TILE_W * JANUS_TILE_H];

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
 * frame has. */
#define JANUS_MAX_ASYNC_OPS 256
typedef enum { JANUS_ASYNC_OP_FILL, JANUS_ASYNC_OP_GLYPH } janus_async_op_kind_t;
typedef struct {
    janus_async_op_kind_t kind;
    int16_t x, y, w, h;            /* GLYPH: w/h unused, always JANUS_FONT_GLYPH_W/H */
    uint16_t color;                /* FILL: the fill value. GLYPH: fg */
    uint16_t bg;                   /* GLYPH only */
    const uint8_t *glyph;          /* GLYPH only */
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

static void async_enqueue_glyph(int16_t x, int16_t y, const uint8_t *glyph, uint16_t fg, uint16_t bg) {
    if (g_async_op_count >= JANUS_MAX_ASYNC_OPS) return;
    janus_async_op_t *op = &g_async_ops[g_async_op_count++];
    op->kind = JANUS_ASYNC_OP_GLYPH;
    op->x = x; op->y = y;
    op->color = fg; op->bg = bg;
    op->glyph = glyph;
}

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
            if (g_async_enqueue) {
                async_enqueue_fill((int16_t)(rect.x + tx), (int16_t)(rect.y + ty), tw, th, value);
            } else {
                draw_area_sync((uint16_t)(rect.x + tx), (uint16_t)(rect.y + ty),
                                (uint16_t)tw, (uint16_t)th, g_tile_buffer);
            }
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

/* -------------------------------------------------------------- glyphs --
 * Reuses g_tile_buffer for the 5x7 = 35 pixels a glyph needs (well inside
 * the 256-pixel tile, same "one shared static scratch buffer, no malloc"
 * discipline as fill_rect above — not a second buffer).
 */
static void draw_glyph(int16_t x, int16_t y, const uint8_t *glyph, uint16_t fg, uint16_t bg) {
    if (g_async_enqueue) {
        async_enqueue_glyph(x, y, glyph, fg, bg);
        return;
    }
    for (int16_t row = 0; row < JANUS_FONT_GLYPH_H; row++) {
        for (int16_t col = 0; col < JANUS_FONT_GLYPH_W; col++) {
            g_tile_buffer[row * JANUS_FONT_GLYPH_W + col] =
                (JANUS_PGM_READ_U8(&glyph[col]) & (1 << row)) ? fg : bg;
        }
    }
    draw_area_sync((uint16_t)x, (uint16_t)y, JANUS_FONT_GLYPH_W, JANUS_FONT_GLYPH_H, g_tile_buffer);
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
 * true) vs. a live bound string (read_bound_string, the vendor's own
 * mutable RAM struct — pass false). Reading a flash string with a plain
 * `*p` (or vice versa) is wrong on classic AVR, so callers must get this
 * right; off-AVR both paths behave identically either way. */
static void draw_string(janus_rect_t rect, const char *text, uint16_t fg, uint16_t bg, bool from_flash) {
    if (text == NULL) return;

    int16_t y = (int16_t)(rect.y + (rect.h - JANUS_FONT_GLYPH_H) / 2);
    if (y < rect.y) y = rect.y;
    int16_t x = (int16_t)(rect.x + 1);
    int16_t right = (int16_t)(rect.x + rect.w);

    for (const char *p = text; ; p++) {
        char c = from_flash ? (char)JANUS_PGM_READ_U8(p) : *p;
        if (c == '\0') break;
        if ((int16_t)(x + JANUS_FONT_GLYPH_W) > right) break;
        const uint8_t *glyph = janus_font_glyph(c);
        if (glyph != NULL) draw_glyph(x, y, glyph, fg, bg);
        x = (int16_t)(x + JANUS_FONT_GLYPH_W + 1);
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

/* --------------------------------------------------------- bound reads --
 */
static double read_bound_value(const janus_bind_t *bind, const void *bound_struct) {
    if (bound_struct == NULL || bind->field_type == JANUS_FIELD_NONE) return 0.0;
    const uint8_t *field = (const uint8_t *)bound_struct + bind->field_offset;
    switch (bind->field_type) {
        case JANUS_FIELD_INT: {
            int v;
            memcpy(&v, field, sizeof(v));
            return (double)v;
        }
        case JANUS_FIELD_INT64: {
            int64_t v;
            memcpy(&v, field, sizeof(v));
            return (double)v;
        }
        case JANUS_FIELD_FLOAT: {
            float v;
            memcpy(&v, field, sizeof(v));
            return (double)v;
        }
        default:
            return 0.0; /* string has no numeric value — see read_bound_string below */
    }
}

/* Slice 2: label/header's bound-string case. The struct field is
 * `const char *` (emit_bindings_struct.py) — a pointer, not inline bytes,
 * so this reads the pointer itself rather than reinterpreting field bytes
 * as a number like read_bound_value does. Zero-initialized instances
 * (Stage 7 — Janus generates shape, not data) hold NULL here until
 * firmware populates them, and draw_string already no-ops on NULL, so an
 * unpopulated bound string renders as the plain fill, same as before this
 * existed. No truncation/copy needed: draw_string blits and clips
 * character-by-character straight from this pointer, so an arbitrary
 * runtime-length string never needs its length known upfront. */
static const char *read_bound_string(const janus_bind_t *bind, const void *bound_struct) {
    if (bound_struct == NULL || bind->field_type != JANUS_FIELD_STRING) return NULL;
    const uint8_t *field = (const uint8_t *)bound_struct + bind->field_offset;
    const char *value;
    memcpy(&value, field, sizeof(value));
    return value;
}

/* ------------------------------------------------- per-kind draw_<kind> --
 * Every widget's fill/text color comes from its own `.color` (ink /
 * foreground / on-state) and `.bg_color` (background / off-state) —
 * authored per-widget in YAML, packed to RGB565 at generation time
 * (emit_embedded_c.py's _pack_rgb565), defaulted by Stage 3b to
 * JANUS_COLOR_DEFAULT_FG/_BG when omitted. Never a runtime constant.
 */

/* label/header: authored `text:` wins if present (unbound widgets, or a
 * widget authored with both — Janus.md's catalog documents `bind`/`text`
 * as one-or-the-other, but nothing at parse time forbids both, so this is
 * the deterministic tie-break); otherwise fall back to the live bound
 * string, if any. */
static void draw_label(const janus_widget_desc_t *w, const void *bound_struct) {
    janus_widget_desc_t lw = janus_widget_load(w);
    fill_rect(lw.geometry, lw.bg_color);
    bool from_flash = lw.static_text != NULL;
    const char *text = from_flash ? lw.static_text : read_bound_string(&lw.bind, bound_struct);
    draw_string(lw.geometry, text, lw.color, lw.bg_color, from_flash);
}
static void draw_header(const janus_widget_desc_t *w, const void *bound_struct) {
    janus_widget_desc_t lw = janus_widget_load(w);
    fill_rect(lw.geometry, lw.bg_color);
    bool from_flash = lw.static_text != NULL;
    const char *text = from_flash ? lw.static_text : read_bound_string(&lw.bind, bound_struct);
    draw_string(lw.geometry, text, lw.color, lw.bg_color, from_flash);
}
static void draw_button(const janus_widget_desc_t *w) {
    janus_widget_desc_t lw = janus_widget_load(w);
    fill_rect(lw.geometry, lw.bg_color);
    draw_string(lw.geometry, lw.static_text, lw.color, lw.bg_color, true);
    if (w == g_focused_widget) draw_focus_ring(lw.geometry);
}
static void draw_image(const janus_widget_desc_t *w) {
    janus_widget_desc_t lw = janus_widget_load(w);
    fill_rect(lw.geometry, lw.color);
}
static void draw_radiobutton(const janus_widget_desc_t *w) {
    janus_widget_desc_t lw = janus_widget_load(w);
    fill_rect(lw.geometry, lw.color);
}
static void draw_divider(const janus_widget_desc_t *w) {
    janus_widget_desc_t lw = janus_widget_load(w);
    fill_rect(lw.geometry, lw.color);
}

static void draw_progress_or_gauge(const janus_widget_desc_t *w, const void *bound_struct) {
    janus_widget_desc_t lw = janus_widget_load(w);
    double value = read_bound_value(&lw.bind, bound_struct);
    double span = (double)lw.bind.range_max - (double)lw.bind.range_min;
    double fraction = span != 0.0 ? (value - lw.bind.range_min) / span : 0.0;
    fill_rect_fraction(lw.geometry, fraction, lw.color, lw.bg_color);
}

static void draw_checkbox(const janus_widget_desc_t *w, const void *bound_struct) {
    janus_widget_desc_t lw = janus_widget_load(w);
    double value = read_bound_value(&lw.bind, bound_struct);
    fill_rect(lw.geometry, value != 0.0 ? lw.color : lw.bg_color);
}

static void draw_led(const janus_widget_desc_t *w, const void *bound_struct) {
    janus_widget_desc_t lw = janus_widget_load(w);
    int state = (int)read_bound_value(&lw.bind, bound_struct);
    uint16_t value = state <= 0 ? lw.bg_color : (state == 1 ? lw.color : JANUS_COLOR_LED_WARN);
    fill_rect(lw.geometry, value);
}

/* toggle/badge/slider intentionally reuse checkbox's and progress/gauge's
 * bind logic exactly (same shape: int on/off, numeric+range) — only the
 * widget kind (and so its own .color/.bg_color) differs, so each reads as
 * its own kind in a render. */
static void draw_toggle(const janus_widget_desc_t *w, const void *bound_struct) {
    janus_widget_desc_t lw = janus_widget_load(w);
    double value = read_bound_value(&lw.bind, bound_struct);
    fill_rect(lw.geometry, value != 0.0 ? lw.color : lw.bg_color);
}

static void draw_badge(const janus_widget_desc_t *w, const void *bound_struct) {
    janus_widget_desc_t lw = janus_widget_load(w);
    double value = read_bound_value(&lw.bind, bound_struct);
    fill_rect(lw.geometry, value != 0.0 ? lw.color : lw.bg_color);
}

static void draw_slider(const janus_widget_desc_t *w, const void *bound_struct) {
    janus_widget_desc_t lw = janus_widget_load(w);
    double value = read_bound_value(&lw.bind, bound_struct);
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
 * read_bound_value already uses for the *value* struct, just a bool
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
    draw_string(lb.geometry_collapsed, lb.static_text, lb.color, lb.bg_color, true);
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

void janus_render_screen(const janus_screen_desc_t *screen) {
    g_current_screen = screen;
    janus_screen_desc_t ls = janus_screen_load(screen);
    for (uint16_t i = 0; i < ls.widget_count; i++) {
        render_widget(&ls.widgets[i], ls.bound_struct, NULL);
    }
}

void janus_render_screen_if_dirty(const janus_screen_desc_t *screen) {
    g_current_screen = screen;
    janus_screen_desc_t ls = janus_screen_load(screen);
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
    } else {
        for (int16_t row = 0; row < JANUS_FONT_GLYPH_H; row++) {
            for (int16_t col = 0; col < JANUS_FONT_GLYPH_W; col++) {
                g_tile_buffer[row * JANUS_FONT_GLYPH_W + col] =
                    (JANUS_PGM_READ_U8(&op->glyph[col]) & (1 << row)) ? op->color : op->bg;
            }
        }
        draw_area_async((uint16_t)op->x, (uint16_t)op->y, JANUS_FONT_GLYPH_W, JANUS_FONT_GLYPH_H, g_tile_buffer);
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
