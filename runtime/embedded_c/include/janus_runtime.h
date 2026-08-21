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

typedef struct {
    uint16_t field_offset;         /* offsetof() into bound_struct; 0 if unbound */
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
    const char *id;
    const char *static_text;           /* authored Widget.text, baked in at generation time; NULL
                                         * if this widget has none, or its text comes from a `bind`
                                         * instead (bound strings don't render yet — janus_font.h's
                                         * v1 slice is static text only, see janus_runtime.c) */
    janus_rect_t geometry;             /* also the "expanded" rect for box */
    janus_rect_t geometry_collapsed;   /* box only, ignored otherwise */
    bool initial_expanded;             /* box only — baked from Widget.default_expanded */
    janus_bind_t bind;
    janus_action_id_t action;          /* on_press only; JANUS_ACTION_ID_NONE otherwise */
    int16_t navigate_target;           /* navigate only; index into janus_app_t.screens, -1 otherwise */
    uint8_t focus_order;               /* encoder/button traversal order, or JANUS_FOCUS_NONE; touch ignores this */
    const struct janus_widget_desc *children;
    uint16_t child_count;
} janus_widget_desc_t;

typedef struct {
    const char *name;
    const janus_widget_desc_t *widgets;
    uint16_t widget_count;
    const void *bound_struct;   /* e.g. &device_instance; NULL if the screen binds nothing */
} janus_screen_desc_t;

typedef struct {
    const janus_screen_desc_t *const *screens;
    const char *const *nav_titles;   /* parallel to screens; NULL if app.nav is unset (no tab bar) */
    uint16_t screen_count;
    uint16_t active_screen;          /* the one piece of app-level runtime state */
} janus_app_t;

/* driver contract, carried forward from the deleted Copilot branch's DESIGN.md.
 * Implemented by vendor/host code, never by the fixed library itself.
 * `pixels` is `w * h` bytes, row-major (row 0 first, left-to-right within
 * a row) — never stated explicitly before janus_font.c became the first
 * caller to draw non-uniform content; every existing fill_rect() call is
 * a uniform memset, so it never depended on an orientation either way. */
void draw_area_sync(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *pixels);
bool draw_area_async(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *pixels);
bool display_busy(void);

/* runtime entry points */
void janus_render_screen(const janus_screen_desc_t *screen);
void janus_switch_screen(janus_app_t *app, uint16_t screen_index);   /* used by navigate */
void janus_toggle_box(const janus_widget_desc_t *box);               /* re-renders just that subtree */

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
