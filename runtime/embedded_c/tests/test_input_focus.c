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
    {
        /* Headerless box (geometry_collapsed.h == 0, no title/summary, no
         * children) -- draw_box_header's `has_strip` branch never runs for
         * it, so nothing in its normal redraw path repaints its footprint.
         * Exercises janus_set_focus's explicit erase for exactly this case
         * (see test_unfocusing_headerless_box_clears_its_full_body_ring). */
        .kind = JANUS_WIDGET_BOX, .id = "box2",
        .geometry = { 60, 90, 40, 30 }, .geometry_collapsed = { 60, 90, 40, 0 },
        .initial_expanded = false,
        .children = NULL, .child_count = 0,
        .navigate_target = -1, .focus_order = 4,
    },
};
#define BUTTON_A (&widgets[0])
#define BUTTON_B (&widgets[2])
#define BOX_WIDGET (&widgets[3])
#define BOX2_WIDGET (&widgets[4])
static const janus_screen_desc_t screen = {
    .name = "Focus", .widgets = widgets, .widget_count = 5, .bound_struct = NULL,
};
/* No `nav:` in this fixture (nav_tabs = NULL) — janus_focus_move/activate
 * take the whole app as of task 4, but with no nav strip every call
 * below degrades to the plain per-screen wrap these tests exercise;
 * test_nav_focus.c covers the nav-folded-into-focus behavior itself. */
static const janus_screen_desc_t *const screens[] = { &screen };
static janus_app_t app = {
    .screens = screens, .screen_count = 1, .active_screen = 0,
    .nav_tabs = NULL, .nav_tab_count = 0, .nav_titles = NULL,
};

static void reset(void) {
    mock_driver_reset();
    janus_render_screen(&screen); /* seeds box state from initial_expanded, sets g_current_screen */
    janus_set_focus(NULL);        /* undo whatever the previous test left focused */
    mock_driver_reset();
}

static void test_delta_zero_establishes_initial_focus_at_first_focusable(void) {
    reset();
    janus_focus_move(&app, 0);
    CHECK(log_has_draw_at(BUTTON_A->geometry.x, BUTTON_A->geometry.y));

    janus_input_result_t hit = janus_focus_activate(&app);
    CHECK(hit.kind == JANUS_INPUT_ACTION);
    CHECK(hit.action == 42);
}

static void test_moving_forward_redraws_old_and_new_focused_widget(void) {
    reset();
    janus_focus_move(&app, 0); /* -> button_a */
    mock_driver_reset();

    janus_focus_move(&app, 1); /* -> button_b */
    CHECK(log_has_draw_at(BUTTON_A->geometry.x, BUTTON_A->geometry.y)); /* old, now unfocused */
    CHECK(log_has_draw_at(BUTTON_B->geometry.x, BUTTON_B->geometry.y)); /* new focus */

    janus_input_result_t hit = janus_focus_activate(&app);
    CHECK(hit.kind == JANUS_INPUT_NAVIGATE);
    CHECK(hit.navigate_target == 5);
}

static void test_moving_forward_wraps_past_the_last_focusable_widget(void) {
    /* box starts collapsed, so box_child isn't reachable yet — the
     * walkable set is exactly {button_a, button_b, box, box2}. */
    reset();
    janus_focus_move(&app, 0); /* -> button_a (index 0) */
    janus_focus_move(&app, 3); /* -> box2 (index 3, last) */

    janus_input_result_t hit = janus_focus_activate(&app);
    CHECK(hit.kind == JANUS_INPUT_TOGGLE_BOX);
    CHECK(hit.widget == BOX2_WIDGET);

    janus_focus_move(&app, 1); /* wraps back to index 0 */
    hit = janus_focus_activate(&app);
    CHECK(hit.kind == JANUS_INPUT_ACTION);
    CHECK(hit.action == 42);
}

static void test_moving_backward_from_first_wraps_to_last(void) {
    reset();
    janus_focus_move(&app, 0); /* -> button_a (index 0) */
    janus_focus_move(&app, -1); /* wraps to index 3: box2 */

    janus_input_result_t hit = janus_focus_activate(&app);
    CHECK(hit.kind == JANUS_INPUT_TOGGLE_BOX);
    CHECK(hit.widget == BOX2_WIDGET);
}

static void test_expanding_box_makes_its_child_reachable(void) {
    reset();
    janus_focus_move(&app, 0); /* -> button_a */
    janus_focus_move(&app, 2); /* -> box */
    janus_toggle_box(BOX_WIDGET); /* now expanded: box_child becomes reachable */

    janus_focus_move(&app, 1); /* -> box_child, not wrapping to button_a anymore */
    janus_input_result_t hit = janus_focus_activate(&app);
    CHECK(hit.kind == JANUS_INPUT_ACTION);
    CHECK(hit.action == 7);
}

static void test_unfocusing_headerless_box_clears_its_full_body_ring(void) {
    reset();
    janus_focus_move(&app, 0);  /* -> button_a */
    janus_focus_move(&app, 3);  /* -> button_b -> box -> box2 */
    CHECK(janus_get_focus() == BOX2_WIDGET);
    mock_driver_reset();

    janus_focus_move(&app, -1); /* -> box; box2 must be unfocus-redrawn to erase its ring */
    CHECK(log_has_draw_at(BOX2_WIDGET->geometry.x, BOX2_WIDGET->geometry.y));
}

static void test_no_focusable_widgets_is_a_defined_no_op(void) {
    static const janus_widget_desc_t label_only = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .geometry = { 0, 0, 10, 10 },
        .navigate_target = -1, .focus_order = JANUS_FOCUS_NONE,
    };
    static const janus_screen_desc_t empty_screen = {
        .name = "Empty", .widgets = &label_only, .widget_count = 1, .bound_struct = NULL,
    };
    static const janus_screen_desc_t *const empty_screens[] = { &empty_screen };
    static janus_app_t empty_app = {
        .screens = empty_screens, .screen_count = 1, .active_screen = 0,
        .nav_tabs = NULL, .nav_tab_count = 0, .nav_titles = NULL,
    };
    mock_driver_reset();
    janus_render_screen(&empty_screen);
    mock_driver_reset();

    janus_focus_move(&empty_app, 1);
    janus_input_result_t hit = janus_focus_activate(&empty_app);
    CHECK(hit.kind == JANUS_INPUT_NONE);
}

int main(void) {
    test_delta_zero_establishes_initial_focus_at_first_focusable();
    test_moving_forward_redraws_old_and_new_focused_widget();
    test_moving_forward_wraps_past_the_last_focusable_widget();
    test_moving_backward_from_first_wraps_to_last();
    /* box2's reachable position assumes `box` (widgets[3]) is still
     * collapsed (box_child not yet focusable) -- must run before
     * test_expanding_box_makes_its_child_reachable, which permanently
     * expands it (box state persists across reset()/janus_render_screen). */
    test_unfocusing_headerless_box_clears_its_full_body_ring();
    test_expanding_box_makes_its_child_reachable(); /* must run after collapsed-state tests */
    test_no_focusable_widgets_is_a_defined_no_op();

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
