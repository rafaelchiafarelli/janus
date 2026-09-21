/* janus_remote.h: get/apply of the UI state a mirror host transfers.
 * Hand-built fixtures in the same style as test_input_focus.c /
 * test_nav_focus.c; draws are observed through the mock driver log. */
#include <stdio.h>

#include "janus_input_focus.h"
#include "janus_remote.h"
#include "janus_runtime.h"
#include "mock_driver.h"

static int g_failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        g_failures++; \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
    } \
} while (0)

/* screen0: button(0)  outer box(1) { inner_btn(2), nested box(3) { deep_btn(4) } }
 * Box ordinals in tree order: outer = 0, nested = 1. Both start collapsed. */
static const janus_widget_desc_t deep_btn = {
    .kind = JANUS_WIDGET_BUTTON, .id = "deep", .geometry = { 0, 120, 40, 20 },
    .action = 9, .navigate_target = -1, .focus_order = 4,
};
static const janus_widget_desc_t outer_children[] = {
    {
        .kind = JANUS_WIDGET_BUTTON, .id = "inner", .geometry = { 0, 80, 40, 20 },
        .action = 8, .navigate_target = -1, .focus_order = 2,
    },
    {
        .kind = JANUS_WIDGET_BOX, .id = "nested",
        .geometry = { 0, 100, 40, 50 }, .geometry_collapsed = { 0, 100, 40, 16 },
        .initial_expanded = false, .children = &deep_btn, .child_count = 1,
        .navigate_target = -1, .focus_order = 3,
    },
};
static const janus_widget_desc_t screen0_widgets[] = {
    {
        .kind = JANUS_WIDGET_BUTTON, .id = "top", .geometry = { 0, 30, 40, 20 },
        .action = 1, .navigate_target = -1, .focus_order = 0,
    },
    {
        .kind = JANUS_WIDGET_BOX, .id = "outer",
        .geometry = { 0, 60, 40, 100 }, .geometry_collapsed = { 0, 60, 40, 16 },
        .initial_expanded = false, .children = outer_children, .child_count = 2,
        .navigate_target = -1, .focus_order = 1,
    },
};
#define OUTER (&screen0_widgets[1])
#define NESTED (&outer_children[1])
static const janus_screen_desc_t screen0 = {
    .name = "S0", .widgets = screen0_widgets, .widget_count = 2, .bound_struct = NULL,
};

static const janus_widget_desc_t screen1_widgets[] = {
    {
        .kind = JANUS_WIDGET_BUTTON, .id = "s1a", .geometry = { 0, 30, 40, 20 },
        .action = 11, .navigate_target = -1, .focus_order = 0,
    },
};
static const janus_screen_desc_t screen1 = {
    .name = "S1", .widgets = screen1_widgets, .widget_count = 1, .bound_struct = NULL,
};

static const janus_nav_tab_t nav_tabs[] = {
    { { 0,  0, 40, 28 }, "S0", 0 },
    { { 40, 0, 40, 28 }, "S1", 1 },
};
static const janus_screen_desc_t *const screens[] = { &screen0, &screen1 };
static janus_app_t app = {
    .screens = screens, .screen_count = 2, .active_screen = 0,
    .nav_tabs = nav_tabs, .nav_tab_count = 2, .nav_titles = NULL,
};

static void reset(void) {
    mock_driver_reset();
    janus_box_set_expanded(OUTER, false);
    janus_box_set_expanded(NESTED, false);
    app.active_screen = 0;
    janus_render_screen(&screen0);   /* seeds g_current_screen + box table */
    janus_set_focus(NULL);
    janus_set_nav_focus(&app, -1);
    mock_driver_reset();
}

static bool same(const janus_remote_state_t *a, const janus_remote_state_t *b) {
    return a->screen == b->screen && a->focus == b->focus &&
           a->nav_focus == b->nav_focus && a->boxes_expanded == b->boxes_expanded;
}

static void test_get_on_a_fresh_ui(void) {
    reset();
    janus_remote_state_t s;
    janus_remote_state_get(&app, &s);
    CHECK(s.screen == 0);
    CHECK(s.focus == -1);
    CHECK(s.nav_focus == -1);
    CHECK(s.boxes_expanded == 0);
}

static void test_get_reports_focus_index_and_box_bits(void) {
    reset();
    janus_box_set_expanded(OUTER, true);
    janus_focus_move(&app, 0);   /* -> top (0) */
    janus_focus_move(&app, 2);   /* -> inner (2): outer expanded, so it is reachable */
    janus_remote_state_t s;
    janus_remote_state_get(&app, &s);
    CHECK(s.focus == 2);
    CHECK(s.boxes_expanded == 0x1);   /* outer = bit 0; nested (bit 1) still collapsed */
}

static void test_get_reports_nav_focus(void) {
    reset();
    janus_set_nav_focus(&app, 1);
    janus_remote_state_t s;
    janus_remote_state_get(&app, &s);
    CHECK(s.nav_focus == 1);
    CHECK(s.focus == -1);
}

static void test_apply_switches_screen_and_redraws(void) {
    reset();
    janus_remote_state_t s = { .screen = 1, .focus = 0, .nav_focus = -1, .boxes_expanded = 0 };
    CHECK(janus_remote_state_apply(&app, &s));
    CHECK(app.active_screen == 1);
    CHECK(mock_driver_log_count > 0);
    janus_remote_state_t got;
    janus_remote_state_get(&app, &got);
    CHECK(same(&got, &s));
}

static void test_apply_round_trips_boxes_and_widget_focus(void) {
    reset();
    /* device: outer + nested expanded, focus on deep_btn (index 4) */
    janus_box_set_expanded(OUTER, true);
    janus_box_set_expanded(NESTED, true);
    janus_focus_move(&app, 0);
    janus_focus_move(&app, 4);
    janus_remote_state_t device;
    janus_remote_state_get(&app, &device);
    CHECK(device.focus == 4);
    CHECK(device.boxes_expanded == 0x3);

    /* viewer: a different UI entirely, then apply the device's state */
    reset();
    CHECK(janus_remote_state_apply(&app, &device));
    janus_remote_state_t viewer;
    janus_remote_state_get(&app, &viewer);
    CHECK(same(&viewer, &device));
    CHECK(janus_box_is_expanded(OUTER));
    CHECK(janus_box_is_expanded(NESTED));
}

static void test_apply_box_bit_order_does_not_depend_on_parent_expansion(void) {
    reset();
    janus_remote_state_t s = { .screen = 0, .focus = -1, .nav_focus = -1, .boxes_expanded = 0x2 };
    CHECK(janus_remote_state_apply(&app, &s));   /* nested (bit 1) expanded, outer (bit 0) not */
    CHECK(!janus_box_is_expanded(OUTER));
    CHECK(janus_box_is_expanded(NESTED));
}

static void test_apply_same_screen_collapses_boxes_and_redraws(void) {
    reset();
    janus_box_set_expanded(OUTER, true);
    janus_remote_state_t open;
    janus_remote_state_get(&app, &open);

    janus_remote_state_t closed = open;
    closed.boxes_expanded = 0;
    mock_driver_reset();
    CHECK(janus_remote_state_apply(&app, &closed));
    CHECK(!janus_box_is_expanded(OUTER));
    CHECK(mock_driver_log_count > 0);   /* erase + redraw even though the screen didn't change */
}

static void test_apply_restores_nav_focus_not_widget_focus(void) {
    reset();
    janus_focus_move(&app, 0);   /* widget focus on `top` */
    janus_remote_state_t s = { .screen = 0, .focus = -1, .nav_focus = 1, .boxes_expanded = 0 };
    CHECK(janus_remote_state_apply(&app, &s));
    CHECK(janus_get_nav_focus() == 1);
    CHECK(janus_get_focus() == NULL);
}

static void test_apply_an_equal_state_draws_nothing(void) {
    reset();
    janus_focus_move(&app, 0);
    janus_remote_state_t s;
    janus_remote_state_get(&app, &s);
    mock_driver_reset();
    CHECK(!janus_remote_state_apply(&app, &s));
    CHECK(mock_driver_log_count == 0);
}

static void test_apply_rejects_out_of_range_states_untouched(void) {
    reset();
    janus_remote_state_t before;
    janus_remote_state_get(&app, &before);

    janus_remote_state_t bad_screen = { .screen = 9, .focus = -1, .nav_focus = -1, .boxes_expanded = 0 };
    janus_remote_state_t bad_nav = { .screen = 0, .focus = -1, .nav_focus = 5, .boxes_expanded = 0 };
    janus_remote_state_t bad_focus = { .screen = 0, .focus = -2, .nav_focus = -1, .boxes_expanded = 0 };
    CHECK(!janus_remote_state_apply(&app, &bad_screen));
    CHECK(!janus_remote_state_apply(&app, &bad_nav));
    CHECK(!janus_remote_state_apply(&app, &bad_focus));
    CHECK(!janus_remote_state_apply(&app, NULL));
    CHECK(mock_driver_log_count == 0);

    janus_remote_state_t after;
    janus_remote_state_get(&app, &after);
    CHECK(same(&before, &after));
}

static void test_apply_focus_past_the_last_widget_redraws_with_nothing_focused(void) {
    reset();
    janus_remote_state_t s = { .screen = 0, .focus = 3, .nav_focus = -1, .boxes_expanded = 0 };
    CHECK(janus_remote_state_apply(&app, &s));   /* only 2 reachable (top, outer): index 3 is past the end */
    CHECK(janus_get_focus() == NULL);
    CHECK(janus_get_nav_focus() == -1);
}

int main(void) {
    test_get_on_a_fresh_ui();
    test_get_reports_focus_index_and_box_bits();
    test_get_reports_nav_focus();
    test_apply_switches_screen_and_redraws();
    test_apply_round_trips_boxes_and_widget_focus();
    test_apply_box_bit_order_does_not_depend_on_parent_expansion();
    test_apply_same_screen_collapses_boxes_and_redraws();
    test_apply_restores_nav_focus_not_widget_focus();
    test_apply_an_equal_state_draws_nothing();
    test_apply_rejects_out_of_range_states_untouched();
    test_apply_focus_past_the_last_widget_redraws_with_nothing_focused();

    if (g_failures != 0) {
        fprintf(stderr, "%d check(s) failed\n", g_failures);
        return 1;
    }
    puts("janus_remote_tests: OK");
    return 0;
}
