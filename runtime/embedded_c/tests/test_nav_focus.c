/* nav_tabs epic task 4: the nav strip folded into janus_input_focus.c's
 * traversal. Same fixture style as test_input_focus.c (hand-built
 * janus_screen_desc_t/janus_nav_tab_t arrays with real focus_order
 * values) plus a two-screen janus_app_t so janus_focus_activate's
 * commit path (janus_switch_screen + re-establish focus) has somewhere
 * real to land.
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

static const janus_widget_desc_t screen0_widgets[] = {
    {
        .kind = JANUS_WIDGET_BUTTON, .id = "s0a", .geometry = { 0, 30, 40, 20 },
        .action = 1, .navigate_target = -1, .focus_order = 0,
    },
    {
        .kind = JANUS_WIDGET_BUTTON, .id = "s0b", .geometry = { 0, 60, 40, 20 },
        .action = 2, .navigate_target = -1, .focus_order = 1,
    },
};
#define S0_A (&screen0_widgets[0])
#define S0_B (&screen0_widgets[1])
static const janus_screen_desc_t screen0 = {
    .name = "S0", .widgets = screen0_widgets, .widget_count = 2, .bound_struct = NULL,
};

static const janus_widget_desc_t screen1_widgets[] = {
    {
        .kind = JANUS_WIDGET_BUTTON, .id = "s1a", .geometry = { 0, 30, 40, 20 },
        .action = 11, .navigate_target = -1, .focus_order = 0,
    },
};
#define S1_A (&screen1_widgets[0])
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
    app.active_screen = 0;
    janus_render_screen(&screen0);   /* seeds g_current_screen */
    janus_set_focus(NULL);
    janus_set_nav_focus(&app, -1);   /* undo whatever the previous test left previewed */
    mock_driver_reset();
}

static void test_forward_past_the_last_widget_lands_on_the_nav_bar(void) {
    reset();
    janus_focus_move(&app, 0); /* -> s0a */
    janus_focus_move(&app, 1); /* -> s0b */
    janus_focus_move(&app, 1); /* -> nav (past the last widget) */

    CHECK(janus_get_focus() == NULL);
    CHECK(janus_get_nav_focus() == 0); /* previewed_tab starts at the tab showing active_screen (0) */
}

static void test_backward_before_the_first_widget_also_lands_on_the_nav_bar(void) {
    reset();
    janus_focus_move(&app, 0);  /* -> s0a */
    janus_focus_move(&app, -1); /* -> nav (before the first widget) */

    CHECK(janus_get_focus() == NULL);
    CHECK(janus_get_nav_focus() == 0);
}

static void test_rotating_on_the_nav_bar_previews_without_switching_screens(void) {
    reset();
    janus_focus_move(&app, 0);
    janus_focus_move(&app, 2); /* -> nav, previewed tab 0 */
    janus_focus_move(&app, 1); /* -> previewed tab 1, still on nav */

    CHECK(janus_get_nav_focus() == 1);
    CHECK(app.active_screen == 0); /* not committed yet */
}

static void test_activate_on_the_nav_bar_commits_the_previewed_tab(void) {
    reset();
    janus_focus_move(&app, 0);
    janus_focus_move(&app, 2); /* -> nav, previewed tab 0 */
    janus_focus_move(&app, 1); /* -> previewed tab 1 */

    janus_input_result_t hit = janus_focus_activate(&app);
    CHECK(hit.kind == JANUS_INPUT_NONE); /* fully handled — nothing left for the caller to dispatch */
    CHECK(app.active_screen == 1);
    CHECK(janus_get_nav_focus() == -1); /* left the nav bar on commit */
    CHECK(janus_get_focus() == S1_A);   /* focus re-established on the new screen */
}

static void test_rotating_backward_off_the_nav_bar_returns_to_the_last_widget(void) {
    reset();
    janus_focus_move(&app, 0);
    janus_focus_move(&app, 2);  /* -> nav, previewed tab 0 */
    janus_focus_move(&app, -1); /* falls off the bottom of the tab run */

    CHECK(janus_get_nav_focus() == -1);
    CHECK(janus_get_focus() == S0_B); /* the last widget on the active screen */
}

static void test_rotating_forward_off_the_nav_bar_returns_to_the_first_widget(void) {
    reset();
    janus_focus_move(&app, 0);
    janus_focus_move(&app, 2); /* -> nav, previewed tab 0 */
    janus_focus_move(&app, 1); /* -> previewed tab 1 (last tab) */
    janus_focus_move(&app, 1); /* falls off the top of the tab run */

    CHECK(janus_get_nav_focus() == -1);
    CHECK(janus_get_focus() == S0_A);
}

static void test_no_focusable_widgets_bootstraps_straight_onto_the_nav_bar(void) {
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
        .nav_tabs = nav_tabs, .nav_tab_count = 2, .nav_titles = NULL,
    };
    mock_driver_reset();
    janus_render_screen(&empty_screen);
    janus_set_focus(NULL);
    janus_set_nav_focus(&empty_app, -1);
    mock_driver_reset();

    janus_focus_move(&empty_app, 0);
    CHECK(janus_get_focus() == NULL);
    CHECK(janus_get_nav_focus() == 0);
}

int main(void) {
    test_forward_past_the_last_widget_lands_on_the_nav_bar();
    test_backward_before_the_first_widget_also_lands_on_the_nav_bar();
    test_rotating_on_the_nav_bar_previews_without_switching_screens();
    test_activate_on_the_nav_bar_commits_the_previewed_tab();
    test_rotating_backward_off_the_nav_bar_returns_to_the_last_widget();
    test_rotating_forward_off_the_nav_bar_returns_to_the_first_widget();
    test_no_focusable_widgets_bootstraps_straight_onto_the_nav_bar();

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
