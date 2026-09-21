#include "janus_input_focus.h"

#include <stddef.h>

static bool is_focusable(const janus_widget_desc_t *w) {
    return janus_widget_load(w).focus_order != JANUS_FOCUS_NONE;
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
    janus_widget_desc_t lw = janus_widget_load(w);
    if (is_focusable(w)) {
        if (visit(w, ctx)) return true;
    }
    if (lw.kind == JANUS_WIDGET_BOX && !janus_box_is_expanded(w)) return false;
    for (uint16_t i = 0; i < lw.child_count; i++) {
        if (walk_focusable(&lw.children[i], visit, ctx)) return true;
    }
    return false;
}

static bool walk_screen(const janus_screen_desc_t *screen, focus_visitor_t visit, void *ctx) {
    janus_screen_desc_t ls = janus_screen_load(screen);
    for (uint16_t i = 0; i < ls.widget_count; i++) {
        if (walk_focusable(&ls.widgets[i], visit, ctx)) return true;
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

/* Index of `app`'s nav_tabs entry whose target is the currently active
 * screen — where the previewed-tab cursor starts on entering the nav
 * strip (nav_tabs epic task 4 decision), same lookup nav_step() in
 * janus_runtime.c does for janus_nav_next/prev, duplicated here rather
 * than shared since both are a three-line loop over a baked array. Only
 * ever called with a non-empty nav_tabs, so falling through the loop
 * (a screen with no matching tab — shouldn't happen; generation always
 * baked one) defaults to tab 0 rather than an out-of-range index. */
static int16_t nav_start_index(const janus_app_t *app) {
    for (uint16_t i = 0; i < app->nav_tab_count; i++) {
        if (janus_nav_tab_load(&app->nav_tabs[i]).target == (int16_t)app->active_screen) return (int16_t)i;
    }
    return 0;
}

void janus_focus_move(janus_app_t *app, int16_t delta) {
    const janus_screen_desc_t *screen = janus_app_get_screen(app, app->active_screen);
    bool has_nav = (app->nav_tabs != NULL && app->nav_tab_count > 0);
    int16_t nav_idx = janus_get_nav_focus();

    if (nav_idx >= 0) {
        /* Already previewing a tab: `delta` cycles it within its own run
         * — independent of the screen's widget count — and falling off
         * either end hands focus back to a real widget instead of
         * wrapping among tabs forever, the mirror of how the strip was
         * entered below. */
        int32_t next = (int32_t)nav_idx + delta;
        count_ctx_t counted = { 0 };
        walk_screen(screen, count_visit, &counted);
        if (next < 0 || next >= (int32_t)app->nav_tab_count) {
            janus_set_nav_focus(app, -1);
            janus_set_focus(counted.count > 0
                             ? widget_at(screen, next < 0 ? counted.count - 1 : 0)
                             : NULL);
            return;
        }
        janus_set_nav_focus(app, (int16_t)next);
        return;
    }

    count_ctx_t counted = { 0 };
    walk_screen(screen, count_visit, &counted);
    if (counted.count <= 0) {
        janus_set_focus(NULL);
        if (has_nav) janus_set_nav_focus(app, nav_start_index(app));
        return;
    }

    int32_t current = focus_position(screen, janus_get_focus());
    /* current == -1 (nothing focused yet, or stale) always lands on
     * index 0 regardless of delta's sign or magnitude — the "establish
     * focus" bootstrap janus_input_focus.h's delta==0 idiom relies on. */
    int32_t next = current < 0 ? 0 : (current + delta);

    if (has_nav && (next < 0 || next >= counted.count)) {
        janus_set_focus(NULL);
        janus_set_nav_focus(app, nav_start_index(app));
        return;
    }
    next = (int32_t)(((next % counted.count) + counted.count) % counted.count);

    janus_set_focus(widget_at(screen, next));
}

int16_t janus_focus_index(const janus_app_t *app) {
    const janus_screen_desc_t *screen = janus_app_get_screen(app, app->active_screen);
    return (int16_t)focus_position(screen, janus_get_focus());
}

bool janus_focus_set_index(janus_app_t *app, int16_t index) {
    if (index < 0) {
        janus_set_focus(NULL);
        return true;
    }
    const janus_screen_desc_t *screen = janus_app_get_screen(app, app->active_screen);
    const janus_widget_desc_t *w = widget_at(screen, index);
    if (w == NULL) return false;
    janus_set_focus(w);
    return true;
}

janus_input_result_t janus_focus_activate(janus_app_t *app) {
    janus_input_result_t result = {
        .kind = JANUS_INPUT_NONE, .widget = NULL,
        .action = JANUS_ACTION_ID_NONE, .navigate_target = -1,
    };

    int16_t nav_idx = janus_get_nav_focus();
    if (nav_idx >= 0) {
        janus_nav_tab_t tab = janus_nav_tab_load(&app->nav_tabs[nav_idx]);
        janus_set_nav_focus(app, -1);
        janus_switch_screen(app, (uint16_t)tab.target);
        janus_focus_move(app, 0);
        return result;
    }

    const janus_screen_desc_t *screen = janus_app_get_screen(app, app->active_screen);
    const janus_widget_desc_t *w = janus_get_focus();
    if (w == NULL || focus_position(screen, w) < 0) return result;

    /* Same resolution rules as janus_input_touch.c's hit_test_widget leaf
     * case — box always toggles; otherwise navigate wins over action if
     * somehow both are set (matches Stage 1's own on_press/navigate
     * handling, not a new tie-break). */
    janus_widget_desc_t lw = janus_widget_load(w);
    if (lw.kind == JANUS_WIDGET_BOX) {
        result.kind = JANUS_INPUT_TOGGLE_BOX;
        result.widget = w;
        return result;
    }
    if (lw.navigate_target >= 0) {
        result.kind = JANUS_INPUT_NAVIGATE;
        result.widget = w;
        result.navigate_target = lw.navigate_target;
        return result;
    }
    if (lw.action != JANUS_ACTION_ID_NONE) {
        result.kind = JANUS_INPUT_ACTION;
        result.widget = w;
        result.action = lw.action;
        return result;
    }
    return result;
}
