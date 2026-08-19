#include "janus_input_touch.h"

static bool point_in_rect(int16_t x, int16_t y, janus_rect_t r) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

static bool hit_test_widget(const janus_widget_desc_t *w, int16_t x, int16_t y,
                             janus_input_result_t *out) {
    if (w->kind == JANUS_WIDGET_BOX) {
        if (point_in_rect(x, y, w->geometry_collapsed)) {
            out->kind = JANUS_INPUT_TOGGLE_BOX;
            out->widget = w;
            return true;
        }
        if (janus_box_is_expanded(w)) {
            for (uint16_t i = 0; i < w->child_count; i++) {
                if (hit_test_widget(&w->children[i], x, y, out)) return true;
            }
        }
        return false;
    }

    /* structural containers — no rect of their own action-wise (Janus.md
     * widget catalog); a hit anywhere in them is really a hit on one of
     * their children, or nothing. */
    if (w->kind == JANUS_WIDGET_COLUMN || w->kind == JANUS_WIDGET_ROW ||
        w->kind == JANUS_WIDGET_RADIOGROUP) {
        for (uint16_t i = 0; i < w->child_count; i++) {
            if (hit_test_widget(&w->children[i], x, y, out)) return true;
        }
        return false;
    }

    /* leaf */
    if (!point_in_rect(x, y, w->geometry)) return false;

    if (w->navigate_target >= 0) {
        out->kind = JANUS_INPUT_NAVIGATE;
        out->widget = w;
        out->navigate_target = w->navigate_target;
        return true;
    }
    if (w->action != JANUS_ACTION_ID_NONE) {
        out->kind = JANUS_INPUT_ACTION;
        out->widget = w;
        out->action = w->action;
        return true;
    }
    /* hit a leaf with no attached behavior (a plain label, an unwired
     * checkbox) — a deliberate, defined miss: nothing to dispatch. */
    return false;
}

janus_input_result_t janus_touch_hit_test(const janus_screen_desc_t *screen, int16_t x, int16_t y) {
    janus_input_result_t result = {
        .kind = JANUS_INPUT_NONE, .widget = NULL,
        .action = JANUS_ACTION_ID_NONE, .navigate_target = -1,
    };
    for (uint16_t i = 0; i < screen->widget_count; i++) {
        if (hit_test_widget(&screen->widgets[i], x, y, &result)) break;
    }
    return result;
}
