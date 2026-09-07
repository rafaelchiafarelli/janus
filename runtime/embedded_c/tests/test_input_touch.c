/* Stage 6 touch hit-test tests — hand-built janus_screen_desc_t
 * fixtures, asserted directly against janus_touch_hit_test()'s result.
 * Same plain-check style as test_runtime.c.
 */
#include <stdio.h>

#include "janus_input_touch.h"
#include "janus_runtime.h"

static int g_failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        g_failures++; \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
    } \
} while (0)

/* Top-level siblings are defined inline in one array (matching what the
 * real Python emitter always produces — see emit_embedded_c.py) rather
 * than as separately-named statics copied in by value: box-state lookup
 * is keyed by descriptor pointer identity, so a copy would have a
 * different address than what janus_touch_hit_test actually walks. */
static const janus_widget_desc_t box_child = {
    .kind = JANUS_WIDGET_BUTTON, .id = "box_child", .geometry = { 0, 106, 50, 20 },
    .action = 99, .navigate_target = -1,
};
static const janus_widget_desc_t widgets[] = {
    {
        .kind = JANUS_WIDGET_BUTTON, .id = "reboot", .geometry = { 0, 0, 50, 20 },
        .action = 42, .navigate_target = -1,
    },
    {
        .kind = JANUS_WIDGET_BUTTON, .id = "goto", .geometry = { 0, 30, 50, 20 },
        .action = JANUS_ACTION_ID_NONE, .navigate_target = 2,
    },
    {
        .kind = JANUS_WIDGET_LABEL, .id = "label", .geometry = { 0, 60, 50, 20 },
        .action = JANUS_ACTION_ID_NONE, .navigate_target = -1,
    },
    {
        .kind = JANUS_WIDGET_BOX, .id = "box",
        .geometry = { 0, 90, 50, 50 }, .geometry_collapsed = { 0, 90, 50, 16 },
        .initial_expanded = false,
        .children = &box_child, .child_count = 1,
        .navigate_target = -1,
    },
};
#define BOX_WIDGET (&widgets[3])
static const janus_screen_desc_t screen = {
    .name = "Touch", .widgets = widgets, .widget_count = 4, .bound_struct = NULL,
};

static void test_tap_on_action_button(void) {
    janus_input_result_t hit = janus_touch_hit_test(&screen, 25, 10);
    CHECK(hit.kind == JANUS_INPUT_ACTION);
    CHECK(hit.action == 42);
}

static void test_tap_on_navigate_button(void) {
    janus_input_result_t hit = janus_touch_hit_test(&screen, 25, 40);
    CHECK(hit.kind == JANUS_INPUT_NAVIGATE);
    CHECK(hit.navigate_target == 2);
}

static void test_tap_on_plain_label_is_a_defined_miss(void) {
    janus_input_result_t hit = janus_touch_hit_test(&screen, 25, 70);
    CHECK(hit.kind == JANUS_INPUT_NONE);
}

static void test_tap_on_empty_space_is_none(void) {
    janus_input_result_t hit = janus_touch_hit_test(&screen, 200, 200);
    CHECK(hit.kind == JANUS_INPUT_NONE);
    CHECK(hit.widget == NULL);
}

static void test_tap_on_box_header_toggles_regardless_of_state(void) {
    janus_input_result_t hit = janus_touch_hit_test(&screen, 25, 95);
    CHECK(hit.kind == JANUS_INPUT_TOGGLE_BOX);
    CHECK(hit.widget == BOX_WIDGET);
}

static void test_collapsed_box_child_is_not_hit_testable(void) {
    /* box_widget starts initial_expanded = false and janus_render_screen
     * was never called on this screen, so the state table seeds from
     * initial_expanded on first query — collapsed. */
    janus_input_result_t hit = janus_touch_hit_test(&screen, 25, 110);
    CHECK(hit.kind == JANUS_INPUT_NONE);
}

static void test_expanded_box_child_becomes_hit_testable(void) {
    janus_toggle_box(BOX_WIDGET); /* flips the runtime-owned state to expanded */

    janus_input_result_t hit = janus_touch_hit_test(&screen, 25, 110);
    CHECK(hit.kind == JANUS_INPUT_ACTION);
    CHECK(hit.action == 99);

    /* header still toggles regardless of current state */
    janus_input_result_t header_hit = janus_touch_hit_test(&screen, 25, 95);
    CHECK(header_hit.kind == JANUS_INPUT_TOGGLE_BOX);
}

/* ---- nav strip hit-test (nav_tabs epic task 3) ---- */
static const janus_nav_tab_t it_nav_tabs[] = {
    { { 0,  0, 40, 28 }, "A", 0 },
    { { 40, 0, 40, 28 }, "B", 1 },
    { { 80, 0, 40, 28 }, "C", 2 },
};
static const janus_screen_desc_t it_nav_screen = { .name = "N", .widget_count = 0 };
static const janus_screen_desc_t *const it_nav_screens[] = {
    &it_nav_screen, &it_nav_screen, &it_nav_screen,
};

static void test_nav_hit_test_resolves_a_tab_tap_to_navigate(void) {
    janus_app_t app = {
        .screens = it_nav_screens, .nav_tabs = it_nav_tabs, .nav_tab_count = 3,
        .screen_count = 3, .active_screen = 0,
    };
    janus_input_result_t hit = janus_nav_hit_test(&app, 100, 14);  /* inside cell C */
    CHECK(hit.kind == JANUS_INPUT_NAVIGATE);
    CHECK(hit.navigate_target == 2);

    /* a point below the 28px band is not the strip's */
    CHECK(janus_nav_hit_test(&app, 100, 40).kind == JANUS_INPUT_NONE);
}

static void test_nav_hit_test_none_without_nav(void) {
    janus_app_t no_nav = {
        .screens = it_nav_screens, .nav_tabs = NULL, .nav_tab_count = 0,
        .screen_count = 3, .active_screen = 0,
    };
    CHECK(janus_nav_hit_test(&no_nav, 10, 10).kind == JANUS_INPUT_NONE);
    CHECK(janus_nav_hit_test(NULL, 10, 10).kind == JANUS_INPUT_NONE);
}

static void test_nav_next_prev_cycle_the_active_screen(void) {
    janus_app_t app = {
        .screens = it_nav_screens, .nav_tabs = it_nav_tabs, .nav_tab_count = 3,
        .screen_count = 3, .active_screen = 0,
    };
    janus_nav_next(&app);  CHECK(app.active_screen == 1);
    janus_nav_next(&app);  CHECK(app.active_screen == 2);
    janus_nav_next(&app);  CHECK(app.active_screen == 0);   /* wraps */
    janus_nav_prev(&app);  CHECK(app.active_screen == 2);   /* wraps back */
}

int main(void) {
    test_tap_on_action_button();
    test_tap_on_navigate_button();
    test_tap_on_plain_label_is_a_defined_miss();
    test_tap_on_empty_space_is_none();
    test_tap_on_box_header_toggles_regardless_of_state();
    test_nav_hit_test_resolves_a_tab_tap_to_navigate();
    test_nav_hit_test_none_without_nav();
    test_nav_next_prev_cycle_the_active_screen();
    test_collapsed_box_child_is_not_hit_testable();
    test_expanded_box_child_becomes_hit_testable(); /* must run last: mutates box state */

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
