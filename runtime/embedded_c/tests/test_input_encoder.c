/* Stage 6 encoder driver-contract tests. janus_input_encoder.h declares
 * no logic of its own (header-only, like draw_area_sync) — this checks
 * the mock driver's queue/poll round-trip, and that a rotate event feeds
 * janus_focus_move correctly end to end (the real wiring the scaffolded
 * main.c does).
 */
#include <stdio.h>

#include "janus_input_encoder.h"
#include "janus_input_focus.h"
#include "janus_runtime.h"
#include "mock_encoder.h"

static int g_failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        g_failures++; \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
    } \
} while (0)

static void test_poll_returns_false_when_queue_is_empty(void) {
    mock_encoder_reset();
    janus_encoder_event_t event;
    int16_t delta;
    CHECK(janus_encoder_poll(&event, &delta) == false);
}

static void test_poll_drains_queued_events_in_order(void) {
    mock_encoder_reset();
    mock_encoder_queue_rotate(3);
    mock_encoder_queue_click();

    janus_encoder_event_t event;
    int16_t delta = 0;
    CHECK(janus_encoder_poll(&event, &delta) == true);
    CHECK(event == JANUS_ENCODER_ROTATE);
    CHECK(delta == 3);

    CHECK(janus_encoder_poll(&event, &delta) == true);
    CHECK(event == JANUS_ENCODER_CLICK);

    CHECK(janus_encoder_poll(&event, &delta) == false);
}

/* End-to-end: the exact rotate -> janus_focus_move wiring main_encoder.c
 * scaffolds. */
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
    .name = "Encoder", .widgets = widgets, .widget_count = 2, .bound_struct = NULL,
};

static void test_rotate_event_moves_focus_via_the_shared_core(void) {
    janus_render_screen(&screen);
    janus_focus_move(&screen, 0); /* establish initial focus, as main_encoder.c does on boot */

    mock_encoder_reset();
    mock_encoder_queue_rotate(1);
    janus_encoder_event_t event;
    int16_t delta;
    CHECK(janus_encoder_poll(&event, &delta) == true);
    CHECK(event == JANUS_ENCODER_ROTATE);
    janus_focus_move(&screen, delta);

    janus_input_result_t hit = janus_focus_activate(&screen);
    CHECK(hit.kind == JANUS_INPUT_ACTION);
    CHECK(hit.action == 20); /* moved from "a" to "b" */
}

int main(void) {
    test_poll_returns_false_when_queue_is_empty();
    test_poll_drains_queued_events_in_order();
    test_rotate_event_moves_focus_via_the_shared_core();

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
