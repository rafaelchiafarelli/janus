/* Janus embedded-C runtime — Stage 4. See architecture.md.
 *
 * Fixed library: hand-written once, shipped with Janus, identical across
 * every generated project. Never templated per-project. The struct shapes
 * here are what Stage 3b's generated descriptor arrays initialize.
 */
#ifndef JANUS_RUNTIME_H
#define JANUS_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>   /* offsetof()/NULL — every generated widget initializer needs these */
#include <stdint.h>

#include "janus_progmem.h"

typedef enum {
    JANUS_WIDGET_LABEL, JANUS_WIDGET_HEADER, JANUS_WIDGET_BUTTON,
    JANUS_WIDGET_IMAGE, JANUS_WIDGET_PROGRESS, JANUS_WIDGET_GAUGE,
    JANUS_WIDGET_CHECKBOX, JANUS_WIDGET_RADIOBUTTON, JANUS_WIDGET_RADIOGROUP,
    JANUS_WIDGET_LED, JANUS_WIDGET_BOX, JANUS_WIDGET_COLUMN, JANUS_WIDGET_ROW,
    JANUS_WIDGET_DIVIDER, JANUS_WIDGET_TOGGLE, JANUS_WIDGET_BADGE, JANUS_WIDGET_SLIDER,
} janus_widget_kind_t;

typedef enum {
    JANUS_FIELD_NONE, JANUS_FIELD_INT, JANUS_FIELD_INT64,
    JANUS_FIELD_FLOAT, JANUS_FIELD_STRING,
} janus_field_type_t;

typedef struct { int16_t x, y, w, h; } janus_rect_t;

/* RGB565: 5 bits red, 6 bits green, 5 bits blue, packed into one 16-bit
 * value — see emit_embedded_c.py's _pack_rgb565 for how a widget's
 * authored "#RRGGBB" becomes one of these at generation time. Stage 3b
 * emits one of these two literally whenever a widget's `color`/`bg` is
 * left unauthored, so every generated descriptor always carries a real
 * color — no runtime-side fallback branch needed. */
#define JANUS_COLOR_DEFAULT_FG ((uint16_t)0x0000)   /* black */
#define JANUS_COLOR_DEFAULT_BG ((uint16_t)0xffff)   /* white */
#define JANUS_COLOR_LED_WARN   ((uint16_t)0xfd80)   /* amber (255,176,0) — led's third state has no authored color (v1) */

typedef struct {
    uint16_t field_offset;         /* offsetof() into bound_struct; 0 if unbound */
    uint16_t dirty_offset;         /* offsetof() into bound_dirty (added 2026-09-05);
                                     * meaningless when field_type == JANUS_FIELD_NONE */
    janus_field_type_t field_type;
    float range_min, range_max;    /* progress/gauge only */
} janus_bind_t;

/* Generic action id, NOT the generated per-project janus_action_t (that
 * type lives in janus_actions.gen.h, which this fixed header can't
 * depend on — see architecture.md Stage 4). Enum constants from the
 * generated header convert into this implicitly. 0 == "no action". */
typedef int16_t janus_action_id_t;
#define JANUS_ACTION_ID_NONE ((janus_action_id_t)0)

/* Stage 6: shared across every input modality (touch, encoder, buttons)
 * — each one's job is just to decide *which* widget, in whatever shape
 * fits its own input (a point vs. a focus index); all three resolve to
 * this same result and get dispatched identically by the caller (the
 * scaffolded main.c's event loop). Originally declared only in
 * janus_input_touch.h (Stage 6's first modality); moved here once
 * encoder/buttons needed the identical shape, so no modality module
 * depends on another. */
typedef enum {
    JANUS_INPUT_NONE,          /* no widget resolved, or nothing to dispatch */
    JANUS_INPUT_ACTION,        /* widget->action is meaningful; caller casts to janus_action_t */
    JANUS_INPUT_NAVIGATE,      /* navigate_target is meaningful; caller calls janus_switch_screen */
    JANUS_INPUT_TOGGLE_BOX,    /* widget is the box that was hit/activated; caller calls janus_toggle_box */
} janus_input_kind_t;

typedef struct {
    janus_input_kind_t kind;
    const struct janus_widget_desc *widget;   /* the resolved widget; NULL when kind == JANUS_INPUT_NONE */
    janus_action_id_t action;          /* valid when kind == JANUS_INPUT_ACTION */
    int16_t navigate_target;           /* valid when kind == JANUS_INPUT_NAVIGATE */
} janus_input_result_t;

/* Stage 6: sentinel for janus_widget_desc_t.focus_order — "not
 * focusable" (touch doesn't need this at all; encoder/button navigation
 * does). Baked at generation time by emit_embedded_c.py's
 * _assign_focus_order, same set of widgets touch already dispatches on
 * (box headers + any leaf with on_press/navigate). */
#define JANUS_FOCUS_NONE ((uint8_t)255)

typedef struct janus_widget_desc {
    janus_widget_kind_t kind;
    const char *id;                    /* generated, flash-resident (JANUS_PROGMEM) on AVR — the
                                         * runtime itself never dereferences this (identity/lookup
                                         * only), but any caller that wants to read/compare it on
                                         * real AVR hardware needs a pgm-aware read (JANUS_PGM_READ_U8
                                         * per byte, or strcmp_P), same as any other flash string;
                                         * plain strcmp/printf("%s", ...) only works on host builds */
    const char *static_text;           /* authored Widget.text, baked in at generation time; NULL
                                         * if this widget has none, or its text comes from a `bind`
                                         * instead (bound strings don't render yet — janus_font.h's
                                         * v1 slice is static text only, see janus_runtime.c).
                                         * Flash-resident (JANUS_PROGMEM) on AVR, same as `id` above
                                         * — draw_string's `from_flash` parameter is what tells it to
                                         * read this one byte-at-a-time via JANUS_PGM_READ_U8, vs. a
                                         * live bound string (read_bound_string), which is always
                                         * plain RAM (the vendor's own mutable struct) and never
                                         * moves. */
    janus_rect_t geometry;             /* also the "expanded" rect for box */
    janus_rect_t geometry_collapsed;   /* box only, ignored otherwise */
    bool initial_expanded;             /* box only — baked from Widget.default_expanded */
    janus_bind_t bind;
    janus_action_id_t action;          /* on_press only; JANUS_ACTION_ID_NONE otherwise */
    int16_t navigate_target;           /* navigate only; index into janus_app_t.screens, -1 otherwise */
    uint8_t focus_order;               /* encoder/button traversal order, or JANUS_FOCUS_NONE; touch ignores this */
    uint16_t color;                    /* RGB565 ink/foreground/on-state fill — see JANUS_COLOR_DEFAULT_FG */
    uint16_t bg_color;                 /* RGB565 background/off-state fill — see JANUS_COLOR_DEFAULT_BG */
    const struct janus_widget_desc *children;
    uint16_t child_count;
    const struct janus_widget_desc *summary_children;  /* box only: always rendered in the
                                                          * header strip, collapsed or expanded
                                                          * — view-only "at a glance" content,
                                                          * distinct from `children` (detail
                                                          * content, expanded-only). NULL/0 for
                                                          * every non-box widget and any box with
                                                          * no `summary:` authored. */
    uint16_t summary_child_count;
} janus_widget_desc_t;

typedef struct {
    const char *name;                  /* flash-resident (JANUS_PROGMEM) on AVR, same caveat as
                                         * janus_widget_desc_t.id above — runtime never reads it */
    const janus_widget_desc_t *widgets;
    uint16_t widget_count;
    const void *bound_struct;   /* e.g. &device_instance; NULL if the screen binds nothing */
    void *bound_dirty;          /* e.g. &device_dirty (added 2026-09-05); mutable — see
                                  * janus_bind_t.dirty_offset and janus_render_*_if_dirty below.
                                  * NULL wherever bound_struct is NULL. */
} janus_screen_desc_t;

typedef struct {
    const janus_screen_desc_t *const *screens;   /* generated as a JANUS_PROGMEM pointer table on
                                                   * AVR — read via janus_app_get_screen(app, i)
                                                   * below, never a plain array index */
    const char *const *nav_titles;   /* parallel to screens; NULL if app.nav is unset (no tab bar) */
    uint16_t screen_count;
    uint16_t active_screen;          /* the one piece of app-level runtime state */
} janus_app_t;

/* ------------------------------------------------------- flash-safe reads --
 * Widget/screen descriptors are generated `JANUS_PROGMEM` (flash-only on
 * AVR, no RAM shadow — see janus_progmem.h). Ordinary pointer dereference
 * can't read flash on classic AVR, so every internal module that used to do
 * `w->field`/`screen->field` directly (janus_runtime.c, janus_input_touch.c,
 * janus_input_focus.c) loads a whole local copy first via these — one
 * JANUS_MEMCPY_P, simpler than a pgm_read_* per field since both structs
 * are flat POD — and reads off that copy instead. Off-AVR this degrades to
 * a plain struct copy, so host behavior (and the host test suite) is
 * unchanged. `static inline` so each translation unit that includes this
 * header gets its own definition, no separate .c/linkage needed. */
static inline janus_widget_desc_t janus_widget_load(const janus_widget_desc_t *w) {
    janus_widget_desc_t out;
    JANUS_MEMCPY_P(&out, w, sizeof(out));
    return out;
}

static inline janus_screen_desc_t janus_screen_load(const janus_screen_desc_t *screen) {
    janus_screen_desc_t out;
    JANUS_MEMCPY_P(&out, screen, sizeof(out));
    return out;
}

/* driver contract, carried forward from the original prototype's DESIGN.md.
 * Implemented by vendor/host code, never by the fixed library itself.
 * `pixels` is `w * h` RGB565 values, row-major (row 0 first, left-to-right
 * within a row) — never stated explicitly before janus_font.c became the
 * first caller to draw non-uniform content; every existing fill_rect()
 * call is a uniform fill, so it never depended on an orientation either
 * way. Pixel type is `uint16_t` (RGB565) — this driver contract targets
 * the RGB565 hardware Janus generates for today; a mono/palette or
 * e-paper target would need its own pixel type and isn't handled by this
 * header (see Janus.md's Open Questions). */
void draw_area_sync(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels);
bool draw_area_async(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels);
bool display_busy(void);

/* app->screens is a JANUS_PROGMEM pointer table on AVR (see janus_app_t
 * above) — this is the one safe way to read an entry out of it. Every
 * scaffolded main.c (janus/templates/main_*.c.tmpl) uses this instead of
 * `janus_app.screens[i]` directly for that reason; a raw index there
 * looked fine on host builds (where the PROGMEM macros are plain memory
 * ops, janus_progmem.h) but fetched a garbage pointer on real AVR
 * hardware. Returns NULL if `index` is out of range. */
const janus_screen_desc_t *janus_app_get_screen(const janus_app_t *app, uint16_t index);

/* runtime entry points */
void janus_render_screen(const janus_screen_desc_t *screen);
void janus_switch_screen(janus_app_t *app, uint16_t screen_index);   /* used by navigate */
void janus_toggle_box(const janus_widget_desc_t *box);               /* re-renders just that subtree */

/* Renders exactly one widget (added 2026-09-05) — and, for a `box`, its
 * always-visible `summary` plus its `children` if currently expanded,
 * same as any other traversal reaching it. The entry point for a caller
 * that wants to refresh one already-known widget on its own cadence
 * (e.g. a persistent header's live fields, redrawn every tick) without a
 * full janus_render_screen sweep repainting the whole screen along with
 * it. `bound_struct` is whatever that widget's screen uses
 * (janus_screen_desc_t.bound_struct) — pass NULL if it has no live
 * bindings anywhere in its subtree. */
void janus_render_widget(const janus_widget_desc_t *widget, const void *bound_struct);

/* Dirty-aware variants (added 2026-09-05) — same traversal as
 * janus_render_screen/janus_render_widget, but a bound leaf is only
 * actually redrawn if its own field's dirty bit (bound_dirty +
 * bind.dirty_offset) is set; the bit is cleared right after that redraw.
 * Firmware sets a field's bit (e.g. `pwm_dirty.ch0_frequency = true`)
 * whenever it writes a new value into the matching `bound_struct` field
 * — nothing here does value comparison, it only trusts what firmware
 * reports changed. Unbound (static) leaves and containers always draw
 * regardless (nothing ever marks them dirty, and they're cheap/one-shot
 * by nature) — this is purely about skipping *unchanged bound data* on a
 * repeated sweep, e.g. a header redrawn on a timer. `janus_toggle_box`/
 * `janus_set_focus` are unaffected — collapse/focus state changing is
 * its own reason to redraw regardless of any field's dirty bit. */
void janus_render_widget_if_dirty(const janus_widget_desc_t *widget, const void *bound_struct, void *bound_dirty);
void janus_render_screen_if_dirty(const janus_screen_desc_t *screen);

/* Non-blocking (polled) rendering — app.yaml's `display.render_mode:
 * non_blocking` (default `blocking`). `janus_render_screen_async_start`
 * computes the screen's draw operations up front (a CPU-only pass, no
 * driver calls) and `janus_render_poll` submits exactly one of them per
 * call via `draw_area_async`, backing off (without advancing) while
 * `display_busy()` — see janus_runtime.c for why this is a queue built
 * once, not a resumable traversal. Call `janus_render_poll` repeatedly
 * (e.g. once per event-loop iteration) until it returns false, meaning
 * the screen is fully drawn. `janus_switch_screen_async_start` is
 * `janus_switch_screen`'s non-blocking counterpart, for the same
 * `navigate` handling under `render_mode: non_blocking`. */
void janus_render_screen_async_start(const janus_screen_desc_t *screen);
bool janus_render_poll(void);
void janus_switch_screen_async_start(janus_app_t *app, uint16_t screen_index);

/* box's current expand/collapse bit, from the runtime-owned state table
 * (the descriptor itself is static const — see janus_runtime.c). Exposed
 * so other modules (Stage 6's janus_input_touch.c) can hit-test against
 * live state without duplicating this table. */
bool janus_box_is_expanded(const janus_widget_desc_t *box);

/* Stage 6: encoder/button focus. `janus_set_focus` draws `widget` in its
 * focused visual state and, if a different widget was previously
 * focused, redraws it unfocused — tile-scoped, same spirit as
 * janus_toggle_box, not a full repaint. `widget` may be NULL to just
 * clear the current highlight (e.g. nothing focusable on this screen).
 * `janus_get_focus` reads it back — used by janus_input_focus.c to find
 * "where am I now" without this module owning any tree-walking logic
 * itself (that stays out of the fixed drawing library, same "runtime
 * draws, input resolves" split janus_input_touch.c already established).
 * janus_switch_screen clears focus before rendering the new screen, so a
 * stale pointer from the old screen's tree is never redrawn. */
void janus_set_focus(const janus_widget_desc_t *widget);
const janus_widget_desc_t *janus_get_focus(void);

#endif /* JANUS_RUNTIME_H */
