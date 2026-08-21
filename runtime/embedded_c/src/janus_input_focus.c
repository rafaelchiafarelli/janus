#include "janus_input_focus.h"

#include <stddef.h>

static bool is_focusable(const janus_widget_desc_t *w) {
    return w->focus_order != JANUS_FOCUS_NONE;
}

/* Depth-first, left-to-right — same order janus_runtime.c's render_widget
 * and janus_input_touch.c's hit_test_widget already traverse in, which is
 * what Stage 3b's _assign_focus_order baked focus_order against. A
 * collapsed box's children are skipped, same rule touch's hit-test uses
 * (architecture.md Stage 6) — focus_order values were baked assuming
 * every box is reachable, so this is what keeps a collapsed box's
 * children from being silently focusable while invisible. `visit`
 * returns true to stop the walk early. */
typedef bool (*focus_visitor_t)(const janus_widget_desc_t *w, void *ctx);

static bool walk_focusable(const janus_widget_desc_t *w, focus_visitor_t visit, void *ctx) {
    if (is_focusable(w)) {
        if (visit(w, ctx)) return true;
    }
    if (w->kind == JANUS_WIDGET_BOX && !janus_box_is_expanded(w)) return false;
    for (uint16_t i = 0; i < w->child_count; i++) {
        if (walk_focusable(&w->children[i], visit, ctx)) return true;
    }
    return false;
}

static bool walk_screen(const janus_screen_desc_t *screen, focus_visitor_t visit, void *ctx) {
    for (uint16_t i = 0; i < screen->widget_count; i++) {
        if (walk_focusable(&screen->widgets[i], visit, ctx)) return true;
    }
    return false;
}

typedef struct { int32_t count; } count_ctx_t;
static bool count_visit(const janus_widget_desc_t *w, void *ctx) {
    (void)w;
    ((count_ctx_t *)ctx)->count++;
    return false; /* never stop early — count every reachable focusable widget */
}

typedef struct { const janus_widget_desc_t *target; int32_t position; int32_t cursor; } locate_ctx_t;
static bool locate_visit(const janus_widget_desc_t *w, void *ctx) {
    locate_ctx_t *c = (locate_ctx_t *)ctx;
    if (w == c->target) { c->position = c->cursor; return true; }
    c->cursor++;
    return false;
}

typedef struct { int32_t target_index; int32_t cursor; const janus_widget_desc_t *found; } at_index_ctx_t;
static bool at_index_visit(const janus_widget_desc_t *w, void *ctx) {
    at_index_ctx_t *c = (at_index_ctx_t *)ctx;
    if (c->cursor == c->target_index) { c->found = w; return true; }
    c->cursor++;
    return false;
}

/* -1 if `w` is NULL or not currently reachable on `screen` (e.g. it was
 * focused inside a box that's since been collapsed, or it belongs to a
 * different screen entirely after a switch). */
static int32_t focus_position(const janus_screen_desc_t *screen, const janus_widget_desc_t *w) {
    if (w == NULL) return -1;
    locate_ctx_t ctx = { w, -1, 0 };
    walk_screen(screen, locate_visit, &ctx);
    return ctx.position;
}

static const janus_widget_desc_t *widget_at(const janus_screen_desc_t *screen, int32_t index) {
    at_index_ctx_t ctx = { index, 0, NULL };
    walk_screen(screen, at_index_visit, &ctx);
    return ctx.found;
}

void janus_focus_move(const janus_screen_desc_t *screen, int16_t delta) {
    count_ctx_t counted = { 0 };
    walk_screen(screen, count_visit, &counted);
    if (counted.count <= 0) {
        janus_set_focus(NULL);
        return;
    }

    int32_t current = focus_position(screen, janus_get_focus());
    /* current == -1 (nothing focused yet, or stale) always lands on
     * index 0 regardless of delta's sign or magnitude — the "establish
     * focus" bootstrap janus_input_focus.h's delta==0 idiom relies on. */
    int32_t next = current < 0 ? 0 : (current + delta);
    next = (int32_t)(((next % counted.count) + counted.count) % counted.count);

    janus_set_focus(widget_at(screen, next));
}

janus_input_result_t janus_focus_activate(const janus_screen_desc_t *screen) {
    janus_input_result_t result = {
        .kind = JANUS_INPUT_NONE, .widget = NULL,
        .action = JANUS_ACTION_ID_NONE, .navigate_target = -1,
    };

    const janus_widget_desc_t *w = janus_get_focus();
    if (w == NULL || focus_position(screen, w) < 0) return result;

    /* Same resolution rules as janus_input_touch.c's hit_test_widget leaf
     * case — box always toggles; otherwise navigate wins over action if
     * somehow both are set (matches Stage 1's own on_press/navigate
     * handling, not a new tie-break). */
    if (w->kind == JANUS_WIDGET_BOX) {
        result.kind = JANUS_INPUT_TOGGLE_BOX;
        result.widget = w;
        return result;
    }
    if (w->navigate_target >= 0) {
        result.kind = JANUS_INPUT_NAVIGATE;
        result.widget = w;
        result.navigate_target = w->navigate_target;
        return result;
    }
    if (w->action != JANUS_ACTION_ID_NONE) {
        result.kind = JANUS_INPUT_ACTION;
        result.widget = w;
        result.action = w->action;
        return result;
    }
    return result;
}
