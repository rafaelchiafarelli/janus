/* Janus embedded-C runtime — Stage 4. See architecture.md.
 *
 * Real traversal + tiling + per-kind dispatch + box state — not the
 * deleted Copilot branch's stub (which drew an empty tile buffer
 * regardless of screen contents). Leaf content used to be a kind-distinct
 * solid fill only; now label/header/button/box-header draw real glyphs
 * over that same fill when a widget has authored `text:` (janus_font.h,
 * slice 1 of 2 — bound string fields still render as a solid fill only,
 * that's slice 2). progress/gauge/checkbox/led remain the pre-existing
 * exception for non-text content — they read the live bound value and
 * vary the fill accordingly.
 */
#include "janus_runtime.h"

#include "janus_font.h"

#include <stddef.h>
#include <string.h>

/* ---------------------------------------------------------- tile buffer --
 * No malloc anywhere. 32x32x1 byte = 1024 B, well under the ~2 KiB
 * transient-buffer budget (Janus.md). Only the synchronous driver path
 * is driven by the traversal so far; draw_area_async/display_busy stay
 * part of the driver contract but aren't called by anything yet —
 * polled/non-blocking scheduling is real future work.
 */
#define JANUS_TILE_W 32
#define JANUS_TILE_H 32
static uint8_t g_tile_buffer[JANUS_TILE_W * JANUS_TILE_H];

static void fill_rect(janus_rect_t rect, uint8_t value) {
    if (rect.w <= 0 || rect.h <= 0) return;
    memset(g_tile_buffer, value, sizeof(g_tile_buffer));

    for (int16_t ty = 0; ty < rect.h; ty += JANUS_TILE_H) {
        int16_t th = (int16_t)(rect.h - ty);
        if (th > JANUS_TILE_H) th = JANUS_TILE_H;
        for (int16_t tx = 0; tx < rect.w; tx += JANUS_TILE_W) {
            int16_t tw = (int16_t)(rect.w - tx);
            if (tw > JANUS_TILE_W) tw = JANUS_TILE_W;
            draw_area_sync((uint16_t)(rect.x + tx), (uint16_t)(rect.y + ty),
                            (uint16_t)tw, (uint16_t)th, g_tile_buffer);
        }
    }
}

/* Splits `rect` into a filled left portion and an empty right portion by
 * `fraction` (clamped to [0, 1]) — used by progress/gauge. */
static void fill_rect_fraction(janus_rect_t rect, double fraction,
                                uint8_t fill_value, uint8_t empty_value) {
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
 * Reuses g_tile_buffer for the 5x7 = 35 bytes a glyph needs (well inside
 * the 1024-byte tile, same "one shared static scratch buffer, no malloc"
 * discipline as fill_rect above — not a second buffer).
 */
#define JANUS_TEXT_FG 0xff  /* lit pixel; ink color is a driver/asset concern, not this runtime's */

static void draw_glyph(int16_t x, int16_t y, const uint8_t *glyph, uint8_t bg) {
    for (int16_t row = 0; row < JANUS_FONT_GLYPH_H; row++) {
        for (int16_t col = 0; col < JANUS_FONT_GLYPH_W; col++) {
            g_tile_buffer[row * JANUS_FONT_GLYPH_W + col] =
                (glyph[col] & (1 << row)) ? JANUS_TEXT_FG : bg;
        }
    }
    draw_area_sync((uint16_t)x, (uint16_t)y, JANUS_FONT_GLYPH_W, JANUS_FONT_GLYPH_H, g_tile_buffer);
}

/* Draws `text` left-aligned, vertically centered in `rect`, over a `bg`
 * that must match whatever solid fill the caller already painted `rect`
 * with (unlit glyph pixels reuse it, so the glyph blends into that
 * backdrop instead of punching a mismatched hole in it). No-op if `text`
 * is NULL (unbound widgets keep rendering as a plain solid fill).
 * Clips, never wraps or shrinks the font, once a character would run
 * past `rect`'s right edge — Janus never auto-sizes text at generation
 * time (Janus.md's deferred auto-sizing note), so overflow here is a
 * real, expected v1 case, not a bug to fix in this runtime. */
static void draw_string(janus_rect_t rect, const char *text, uint8_t bg) {
    if (text == NULL) return;

    int16_t y = (int16_t)(rect.y + (rect.h - JANUS_FONT_GLYPH_H) / 2);
    if (y < rect.y) y = rect.y;
    int16_t x = (int16_t)(rect.x + 1);
    int16_t right = (int16_t)(rect.x + rect.w);

    for (const char *p = text; *p != '\0'; p++) {
        if ((int16_t)(x + JANUS_FONT_GLYPH_W) > right) break;
        const uint8_t *glyph = janus_font_glyph(*p);
        if (glyph != NULL) draw_glyph(x, y, glyph, bg);
        x = (int16_t)(x + JANUS_FONT_GLYPH_W + 1);
    }
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
        slot->expanded = box->initial_expanded;
        return slot;
    }
    return NULL; /* table full: caller falls back to initial_expanded, never toggles */
}

bool janus_box_is_expanded(const janus_widget_desc_t *box) {
    janus_box_state_t *slot = box_state_find_or_register(box);
    return slot != NULL ? slot->expanded : box->initial_expanded;
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
            return 0.0; /* string has no numeric value; no glyph rendering yet anyway */
    }
}

/* ------------------------------------------------- per-kind draw_<kind> --
 * Kind-distinct placeholder fill bytes, still the *only* content for any
 * widget with no authored `text:` (unbound or bound-string leaves) — see
 * janus_font.h for what draws over this backdrop when text is present.
 */
enum {
    FILL_LABEL = 0x10, FILL_HEADER = 0x20, FILL_BUTTON = 0x30, FILL_IMAGE = 0x40,
    FILL_RADIOBUTTON = 0x50, FILL_BOX_HEADER = 0x60,
    FILL_ON = 0x70, FILL_OFF = 0x18,
    FILL_LED_OFF = 0x08, FILL_LED_ON = 0x80, FILL_LED_WARN = 0xC0,
    FILL_DIVIDER = 0x90,
    FILL_TOGGLE_ON = 0xA0, FILL_TOGGLE_OFF = 0xA8,
    FILL_BADGE_ON = 0xB0, FILL_BADGE_OFF = 0xB8,
    FILL_SLIDER_ON = 0xC8, FILL_SLIDER_OFF = 0xD0,
};

static void draw_label(const janus_widget_desc_t *w) {
    fill_rect(w->geometry, FILL_LABEL);
    draw_string(w->geometry, w->static_text, FILL_LABEL);
}
static void draw_header(const janus_widget_desc_t *w) {
    fill_rect(w->geometry, FILL_HEADER);
    draw_string(w->geometry, w->static_text, FILL_HEADER);
}
static void draw_button(const janus_widget_desc_t *w) {
    fill_rect(w->geometry, FILL_BUTTON);
    draw_string(w->geometry, w->static_text, FILL_BUTTON);
}
static void draw_image(const janus_widget_desc_t *w) { fill_rect(w->geometry, FILL_IMAGE); }
static void draw_radiobutton(const janus_widget_desc_t *w) { fill_rect(w->geometry, FILL_RADIOBUTTON); }
static void draw_divider(const janus_widget_desc_t *w) { fill_rect(w->geometry, FILL_DIVIDER); }

static void draw_progress_or_gauge(const janus_widget_desc_t *w, const void *bound_struct) {
    double value = read_bound_value(&w->bind, bound_struct);
    double span = (double)w->bind.range_max - (double)w->bind.range_min;
    double fraction = span != 0.0 ? (value - w->bind.range_min) / span : 0.0;
    fill_rect_fraction(w->geometry, fraction, FILL_ON, FILL_OFF);
}

static void draw_checkbox(const janus_widget_desc_t *w, const void *bound_struct) {
    double value = read_bound_value(&w->bind, bound_struct);
    fill_rect(w->geometry, value != 0.0 ? FILL_ON : FILL_OFF);
}

static void draw_led(const janus_widget_desc_t *w, const void *bound_struct) {
    int state = (int)read_bound_value(&w->bind, bound_struct);
    uint8_t value = state <= 0 ? FILL_LED_OFF : (state == 1 ? FILL_LED_ON : FILL_LED_WARN);
    fill_rect(w->geometry, value);
}

/* toggle/badge/slider intentionally reuse checkbox's and progress/gauge's
 * bind logic exactly (same shape: int on/off, numeric+range) — only the
 * fill bytes differ, so each reads as its own kind in a render. */
static void draw_toggle(const janus_widget_desc_t *w, const void *bound_struct) {
    double value = read_bound_value(&w->bind, bound_struct);
    fill_rect(w->geometry, value != 0.0 ? FILL_TOGGLE_ON : FILL_TOGGLE_OFF);
}

static void draw_badge(const janus_widget_desc_t *w, const void *bound_struct) {
    double value = read_bound_value(&w->bind, bound_struct);
    fill_rect(w->geometry, value != 0.0 ? FILL_BADGE_ON : FILL_BADGE_OFF);
}

static void draw_slider(const janus_widget_desc_t *w, const void *bound_struct) {
    double value = read_bound_value(&w->bind, bound_struct);
    double span = (double)w->bind.range_max - (double)w->bind.range_min;
    double fraction = span != 0.0 ? (value - w->bind.range_min) / span : 0.0;
    fill_rect_fraction(w->geometry, fraction, FILL_SLIDER_ON, FILL_SLIDER_OFF);
}

/* box's own content is just its header strip; children are separate
 * descriptors, drawn (or not) by the traversal below. Its title text is
 * `box.static_text` — box has no dedicated title field, it reuses the
 * generic Widget.text (Janus.md's widget catalog / architecture.md
 * Stage 2). */
static void draw_box_header(const janus_widget_desc_t *box) {
    fill_rect(box->geometry_collapsed, FILL_BOX_HEADER);
    draw_string(box->geometry_collapsed, box->static_text, FILL_BOX_HEADER);
}

/* ---------------------------------------------------------- traversal --
 */
static void render_widget(const janus_widget_desc_t *w, const void *bound_struct) {
    switch (w->kind) {
        case JANUS_WIDGET_LABEL: draw_label(w); return;
        case JANUS_WIDGET_HEADER: draw_header(w); return;
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
            draw_box_header(w);
            if (janus_box_is_expanded(w)) {
                for (uint16_t i = 0; i < w->child_count; i++) {
                    render_widget(&w->children[i], bound_struct);
                }
            }
            return;

        /* structural containers — no pixels of their own (Janus.md widget catalog) */
        case JANUS_WIDGET_COLUMN:
        case JANUS_WIDGET_ROW:
        case JANUS_WIDGET_RADIOGROUP:
            for (uint16_t i = 0; i < w->child_count; i++) {
                render_widget(&w->children[i], bound_struct);
            }
            return;
    }
}

void janus_render_screen(const janus_screen_desc_t *screen) {
    g_current_screen = screen;
    for (uint16_t i = 0; i < screen->widget_count; i++) {
        render_widget(&screen->widgets[i], screen->bound_struct);
    }
}

void janus_switch_screen(janus_app_t *app, uint16_t screen_index) {
    if (screen_index >= app->screen_count) return;
    app->active_screen = screen_index;
    janus_render_screen(app->screens[screen_index]);
}

void janus_toggle_box(const janus_widget_desc_t *box) {
    janus_box_state_t *slot = box_state_find_or_register(box);
    if (slot != NULL) {
        slot->expanded = !slot->expanded;
    }

    const void *bound_struct = g_current_screen != NULL ? g_current_screen->bound_struct : NULL;
    draw_box_header(box);
    if (janus_box_is_expanded(box)) {
        for (uint16_t i = 0; i < box->child_count; i++) {
            render_widget(&box->children[i], bound_struct);
        }
    }
}
