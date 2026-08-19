/* Janus embedded-C runtime — Stage 6, touch modality. See architecture.md.
 *
 * Fixed library, one module per input modality (this is the first;
 * encoder/buttons stay deferred). Resolves a touch point to what should
 * happen, using the geometry Stage 3b already baked in — it does not
 * decide *how* to dispatch an on_press action, since that requires the
 * generated, per-project janus_action_t this fixed module can't depend
 * on (see janus_action_id_t in janus_runtime.h). The caller — normally
 * the scaffolded main.c's event loop — does that final step.
 */
#ifndef JANUS_INPUT_TOUCH_H
#define JANUS_INPUT_TOUCH_H

#include "janus_runtime.h"

typedef enum {
    JANUS_INPUT_NONE,          /* no widget at that point, or nothing to dispatch */
    JANUS_INPUT_ACTION,        /* widget->action is meaningful; caller casts to janus_action_t */
    JANUS_INPUT_NAVIGATE,      /* navigate_target is meaningful; caller calls janus_switch_screen */
    JANUS_INPUT_TOGGLE_BOX,    /* widget is the box that was tapped; caller calls janus_toggle_box */
} janus_input_kind_t;

typedef struct {
    janus_input_kind_t kind;
    const janus_widget_desc_t *widget;   /* the resolved widget; NULL when kind == JANUS_INPUT_NONE */
    janus_action_id_t action;            /* valid when kind == JANUS_INPUT_ACTION */
    int16_t navigate_target;             /* valid when kind == JANUS_INPUT_NAVIGATE */
} janus_input_result_t;

/* Point-in-rect against `screen`'s already-baked absolute geometry,
 * deepest match wins. A box's header region (geometry_collapsed) is
 * always live; its children are only hit-testable while
 * janus_box_is_expanded() is true. */
janus_input_result_t janus_touch_hit_test(const janus_screen_desc_t *screen, int16_t x, int16_t y);

/* Input driver contract — vendor/host-provided, non-blocking, mirrors
 * display_busy()'s polling style. Returns true and fills x/y exactly
 * once per new touch; false if there's nothing new since the last call. */
bool janus_touch_poll(int16_t *x, int16_t *y);

#endif /* JANUS_INPUT_TOUCH_H */
