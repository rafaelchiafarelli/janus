/* Stage 6 push-button driver-contract tests. janus_input_buttons.h
 * declares no logic of its own (header-only) — this checks the mock
 * driver's queue/poll round-trip, and that NEXT/PREV/SELECT feed the
 * shared focus core correctly end to end (the real wiring the scaffolded
 * main.c does).
 */
#include <stdio.h>

#include "janus_input_buttons.h"
#include "janus_input_focus.h"
#include "janus_runtime.h"
#include "mock_buttons.h"

static int g_failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        g_failures++; \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
    } \
} while (0)

static void test_poll_returns_false_when_queue_is_empty(void) {
    mock_buttons_reset();
    janus_button_event_t event;
    CHECK(janus_buttons_poll(&event) == false);
}

static void test_poll_drains_queued_events_in_order(void) {
    mock_buttons_reset();
    mock_buttons_queue(JANUS_BUTTON_NEXT);
    mock_buttons_queue(JANUS_BUTTON_SELECT);

    janus_button_event_t event;
    CHECK(janus_buttons_poll(&event) == true);
    CHECK(event == JANUS_BUTTON_NEXT);

    CHECK(janus_buttons_poll(&event) == true);
    CHECK(event == JANUS_BUTTON_SELECT);

    CHECK(janus_buttons_poll(&event) == false);
}

/* End-to-end: the exact NEXT/PREV/SELECT -> janus_focus_move/activate
 * wiring main_buttons.c scaffolds. */
static const janus_widget_desc_t widgets[] = {
    {
        .kind = JANUS_WIDGET_BUTTON, .id = "a", .geometry = { 0, 0, 40, 20 },
        .action = 10, .navigate_target = -1, .focus_order = 0,
    },
    {
        .kind = JANUS_WIDGET_BUTTON, .id = "b", .geometry = { 0, 30, 40, 20 },
        .action = 20, .navigate_target = -1, .focus_order = 1,
    },
};
static const janus_screen_desc_t screen = {
    .name = "Buttons", .widgets = widgets, .widget_count = 2, .bound_struct = NULL,
};

static void test_next_then_select_activates_the_second_widget(void) {
    janus_render_screen(&screen);
    janus_focus_move(&screen, 0); /* establish initial focus, as main_buttons.c does on boot */

    mock_buttons_reset();
    mock_buttons_queue(JANUS_BUTTON_NEXT);
    mock_buttons_queue(JANUS_BUTTON_SELECT);

    janus_button_event_t event;
    CHECK(janus_buttons_poll(&event) == true);
    CHECK(event == JANUS_BUTTON_NEXT);
    janus_focus_move(&screen, 1);

    CHECK(janus_buttons_poll(&event) == true);
    CHECK(event == JANUS_BUTTON_SELECT);
    janus_input_result_t hit = janus_focus_activate(&screen);
    CHECK(hit.kind == JANUS_INPUT_ACTION);
    CHECK(hit.action == 20);
}

static void test_prev_wraps_backward(void) {
    janus_render_screen(&screen);
    janus_set_focus(NULL);         /* clear whatever a previous test left focused */
    janus_focus_move(&screen, 0);  /* -> "a" (nothing focused, so delta==0 lands on index 0) */
    janus_focus_move(&screen, -1); /* PREV from "a" wraps to "b" */

    janus_input_result_t hit = janus_focus_activate(&screen);
    CHECK(hit.kind == JANUS_INPUT_ACTION);
    CHECK(hit.action == 20);
}

int main(void) {
    test_poll_returns_false_when_queue_is_empty();
    test_poll_drains_queued_events_in_order();
    test_next_then_select_activates_the_second_widget();
    test_prev_wraps_backward();

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
