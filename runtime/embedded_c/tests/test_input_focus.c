/* Stage 6 shared focus-core tests (janus_input_focus.h) — hand-built
 * janus_screen_desc_t fixtures with real .focus_order values, same style
 * as test_input_touch.c. Exercises janus_focus_move/janus_focus_activate
 * directly; janus_set_focus's visual redraw is checked through the mock
 * driver log (its exact fill byte is a janus_runtime.c implementation
 * detail, not part of any header, so these check redrawn *geometry*
 * instead — the log's first tile for a widget always starts at that
 * widget's own rect.x/rect.y).
 */
#include <stdio.h>

#include "janus_input_focus.h"
#include "janus_runtime.h"
#include "mock_driver.h"

static int g_failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        g_failures++; \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
    } \
} while (0)

static bool log_has_draw_at(int16_t x, int16_t y) {
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        if (mock_driver_log[i].x == x && mock_driver_log[i].y == y) return true;
    }
    return false;
}

/* button_a(0) -> label_mid(NONE) -> button_b(1) -> box(2) -> box_child(3,
 * only reachable once box is expanded) — same "inline array, pointer
 * identity matters" reasoning as test_input_touch.c's fixture. */
static const janus_widget_desc_t box_child = {
    .kind = JANUS_WIDGET_BUTTON, .id = "box_child", .geometry = { 0, 140, 40, 20 },
    .action = 7, .navigate_target = -1, .focus_order = 3,
};
static const janus_widget_desc_t widgets[] = {
    {
        .kind = JANUS_WIDGET_BUTTON, .id = "button_a", .geometry = { 0, 0, 40, 20 },
        .action = 42, .navigate_target = -1, .focus_order = 0,
    },
    {
        .kind = JANUS_WIDGET_LABEL, .id = "label_mid", .geometry = { 0, 30, 40, 20 },
        .navigate_target = -1, .focus_order = JANUS_FOCUS_NONE,
    },
    {
        .kind = JANUS_WIDGET_BUTTON, .id = "button_b", .geometry = { 0, 60, 40, 20 },
        .action = JANUS_ACTION_ID_NONE, .navigate_target = 5, .focus_order = 1,
    },
    {
        .kind = JANUS_WIDGET_BOX, .id = "box",
        .geometry = { 0, 90, 40, 70 }, .geometry_collapsed = { 0, 90, 40, 16 },
        .initial_expanded = false,
        .children = &box_child, .child_count = 1,
        .navigate_target = -1, .focus_order = 2,
    },
};
#define BUTTON_A (&widgets[0])
#define BUTTON_B (&widgets[2])
#define BOX_WIDGET (&widgets[3])
static const janus_screen_desc_t screen = {
    .name = "Focus", .widgets = widgets, .widget_count = 4, .bound_struct = NULL,
};

static void reset(void) {
    mock_driver_reset();
    janus_render_screen(&screen); /* seeds box state from initial_expanded, sets g_current_screen */
    janus_set_focus(NULL);        /* undo whatever the previous test left focused */
    mock_driver_reset();
}

static void test_delta_zero_establishes_initial_focus_at_first_focusable(void) {
    reset();
    janus_focus_move(&screen, 0);
    CHECK(log_has_draw_at(BUTTON_A->geometry.x, BUTTON_A->geometry.y));

    janus_input_result_t hit = janus_focus_activate(&screen);
    CHECK(hit.kind == JANUS_INPUT_ACTION);
    CHECK(hit.action == 42);
}

static void test_moving_forward_redraws_old_and_new_focused_widget(void) {
    reset();
    janus_focus_move(&screen, 0); /* -> button_a */
    mock_driver_reset();

    janus_focus_move(&screen, 1); /* -> button_b */
    CHECK(log_has_draw_at(BUTTON_A->geometry.x, BUTTON_A->geometry.y)); /* old, now unfocused */
    CHECK(log_has_draw_at(BUTTON_B->geometry.x, BUTTON_B->geometry.y)); /* new focus */

    janus_input_result_t hit = janus_focus_activate(&screen);
    CHECK(hit.kind == JANUS_INPUT_NAVIGATE);
    CHECK(hit.navigate_target == 5);
}

static void test_moving_forward_wraps_past_the_last_focusable_widget(void) {
    /* box starts collapsed, so box_child isn't reachable yet — the
     * walkable set is exactly {button_a, button_b, box}. */
    reset();
    janus_focus_move(&screen, 0); /* -> button_a (index 0) */
    janus_focus_move(&screen, 2); /* -> box (index 2) */

    janus_input_result_t hit = janus_focus_activate(&screen);
    CHECK(hit.kind == JANUS_INPUT_TOGGLE_BOX);
    CHECK(hit.widget == BOX_WIDGET);

    janus_focus_move(&screen, 1); /* wraps back to index 0 */
    hit = janus_focus_activate(&screen);
    CHECK(hit.kind == JANUS_INPUT_ACTION);
    CHECK(hit.action == 42);
}

static void test_moving_backward_from_first_wraps_to_last(void) {
    reset();
    janus_focus_move(&screen, 0); /* -> button_a (index 0) */
    janus_focus_move(&screen, -1); /* wraps to index 2: box */

    janus_input_result_t hit = janus_focus_activate(&screen);
    CHECK(hit.kind == JANUS_INPUT_TOGGLE_BOX);
}

static void test_expanding_box_makes_its_child_reachable(void) {
    reset();
    janus_focus_move(&screen, 0); /* -> button_a */
    janus_focus_move(&screen, 2); /* -> box */
    janus_toggle_box(BOX_WIDGET); /* now expanded: box_child becomes reachable */

    janus_focus_move(&screen, 1); /* -> box_child, not wrapping to button_a anymore */
    janus_input_result_t hit = janus_focus_activate(&screen);
    CHECK(hit.kind == JANUS_INPUT_ACTION);
    CHECK(hit.action == 7);
}

static void test_no_focusable_widgets_is_a_defined_no_op(void) {
    static const janus_widget_desc_t label_only = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .geometry = { 0, 0, 10, 10 },
        .navigate_target = -1, .focus_order = JANUS_FOCUS_NONE,
    };
    static const janus_screen_desc_t empty_screen = {
        .name = "Empty", .widgets = &label_only, .widget_count = 1, .bound_struct = NULL,
    };
    mock_driver_reset();
    janus_render_screen(&empty_screen);
    mock_driver_reset();

    janus_focus_move(&empty_screen, 1);
    janus_input_result_t hit = janus_focus_activate(&empty_screen);
    CHECK(hit.kind == JANUS_INPUT_NONE);
}

int main(void) {
    test_delta_zero_establishes_initial_focus_at_first_focusable();
    test_moving_forward_redraws_old_and_new_focused_widget();
    test_moving_forward_wraps_past_the_last_focusable_widget();
    test_moving_backward_from_first_wraps_to_last();
    test_expanding_box_makes_its_child_reachable(); /* must run after collapsed-state tests */
    test_no_focusable_widgets_is_a_defined_no_op();

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
