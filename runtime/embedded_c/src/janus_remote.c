#include "janus_remote.h"

#include <stddef.h>

#include "janus_input_focus.h"

#define REMOTE_MAX_BOXES 16

/* Depth-first over the whole tree, counting *every* box regardless of
 * whether its parent is expanded, so a box's ordinal never depends on the
 * expansion state being transferred. */
static void boxes_get(const janus_widget_desc_t *w, uint16_t *ordinal, uint16_t *mask) {
    janus_widget_desc_t lw = janus_widget_load(w);
    if (lw.kind == JANUS_WIDGET_BOX) {
        if (*ordinal < REMOTE_MAX_BOXES && janus_box_is_expanded(w)) *mask |= (uint16_t)(1u << *ordinal);
        (*ordinal)++;
    }
    for (uint16_t i = 0; i < lw.child_count; i++) boxes_get(&lw.children[i], ordinal, mask);
}

static void boxes_set(const janus_widget_desc_t *w, uint16_t *ordinal, uint16_t mask) {
    janus_widget_desc_t lw = janus_widget_load(w);
    if (lw.kind == JANUS_WIDGET_BOX) {
        if (*ordinal < REMOTE_MAX_BOXES) janus_box_set_expanded(w, (mask >> *ordinal) & 1u);
        (*ordinal)++;
    }
    for (uint16_t i = 0; i < lw.child_count; i++) boxes_set(&lw.children[i], ordinal, mask);
}

void janus_remote_state_get(const janus_app_t *app, janus_remote_state_t *out) {
    janus_screen_desc_t screen = janus_screen_load(janus_app_get_screen(app, app->active_screen));
    uint16_t ordinal = 0, mask = 0;
    for (uint16_t i = 0; i < screen.widget_count; i++) boxes_get(&screen.widgets[i], &ordinal, &mask);

    out->screen = app->active_screen;
    out->focus = janus_focus_index(app);
    out->nav_focus = janus_get_nav_focus();
    out->boxes_expanded = mask;
}

bool janus_remote_state_apply(janus_app_t *app, const janus_remote_state_t *state) {
    if (app == NULL || state == NULL) return false;
    if (state->screen >= app->screen_count) return false;
    if (state->focus < -1 || state->nav_focus < -1) return false;
    if (state->nav_focus >= 0 &&
        (app->nav_tabs == NULL || (uint16_t)state->nav_focus >= app->nav_tab_count)) return false;

    janus_remote_state_t current;
    janus_remote_state_get(app, &current);
    if (current.screen == state->screen && current.focus == state->focus &&
        current.nav_focus == state->nav_focus && current.boxes_expanded == state->boxes_expanded) {
        return false;
    }

    /* Box states first (state only, no drawing) so the redraw below and the
     * focus traversal after it both see the target expansion. */
    janus_screen_desc_t target = janus_screen_load(janus_app_get_screen(app, state->screen));
    uint16_t ordinal = 0;
    for (uint16_t i = 0; i < target.widget_count; i++) boxes_set(&target.widgets[i], &ordinal, state->boxes_expanded);

    /* janus_switch_screen clears widget focus itself, but not the nav
     * strip's preview — and it erases + fully redraws even when `screen`
     * is already the active one, which is what a box/focus-only change needs. */
    janus_set_nav_focus(app, -1);
    janus_switch_screen(app, state->screen);

    if (state->nav_focus >= 0) {
        janus_set_nav_focus(app, state->nav_focus);
    } else {
        janus_focus_set_index(app, state->focus);   /* out-of-range: nothing focused */
    }
    return true;
}
