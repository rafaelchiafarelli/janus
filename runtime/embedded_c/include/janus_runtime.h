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

typedef struct janus_widget_desc {
    janus_widget_kind_t kind;
    const char *id;
    janus_rect_t geometry;             /* also the "expanded" rect for box */
    janus_rect_t geometry_collapsed;   /* box only, ignored otherwise */
    bool initial_expanded;             /* box only — baked from Widget.default_expanded */
    janus_bind_t bind;
    janus_action_id_t action;          /* on_press only; JANUS_ACTION_ID_NONE otherwise */
    int16_t navigate_target;           /* navigate only; index into janus_app_t.screens, -1 otherwise */
    uint8_t focus_order;               /* input dispatch — reserved, unused until Stage 6 lands */
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
 * Implemented by vendor/host code, never by the fixed library itself. */
void draw_area_sync(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *pixels);
bool draw_area_async(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *pixels);
bool display_busy(void);

/* runtime entry points */
void janus_render_screen(const janus_screen_desc_t *screen);
void janus_switch_screen(janus_app_t *app, uint16_t screen_index);   /* used by navigate */
void janus_toggle_box(const janus_widget_desc_t *box);               /* re-renders just that subtree */

#endif /* JANUS_RUNTIME_H */
