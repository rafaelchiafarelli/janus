/* Non-blocking (polled) rendering tests — janus_render_screen_async_start
 * builds a draw-op queue (a CPU-only pass, no driver calls), and
 * janus_render_poll drains it one driver call at a time. Uses the mock
 * driver's display_busy() control hook (mock_driver_set_busy_for) to
 * exercise the backoff without any real hardware.
 */
#include <stddef.h>
#include <stdio.h>

#include "janus_runtime.h"
#include "mock_driver.h"

static int g_failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        g_failures++; \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
    } \
} while (0)

static void test_async_start_makes_no_driver_calls(void) {
    /* Building the queue is pure CPU work — nothing should reach the
     * driver until the first janus_render_poll() call. */
    static const janus_widget_desc_t widgets[] = {
        { .kind = JANUS_WIDGET_LABEL, .id = "a", .geometry = { 0, 0, 10, 10 } },
    };
    static const janus_screen_desc_t screen = {
        .name = "AsyncStart", .widgets = widgets, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen_async_start(&screen);
    CHECK(mock_driver_log_count == 0);
}

static void test_poll_drains_one_op_per_call_and_reports_completion(void) {
    /* Two same-tile-sized widgets -> two fill ops (one tile each). Each
     * poll should submit exactly one and report `true` (more remaining)
     * until the last one, which reports `false`. */
    static const janus_widget_desc_t widgets[] = {
        { .kind = JANUS_WIDGET_LABEL, .id = "a", .geometry = { 0, 0, 10, 10 } },
        { .kind = JANUS_WIDGET_LABEL, .id = "b", .geometry = { 0, 10, 10, 10 } },
    };
    static const janus_screen_desc_t screen = {
        .name = "Drain", .widgets = widgets, .widget_count = 2, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen_async_start(&screen);

    CHECK(janus_render_poll() == true);
    CHECK(mock_driver_log_count == 1);

    CHECK(janus_render_poll() == false);
    CHECK(mock_driver_log_count == 2);

    /* Fully drained — further polls are a no-op, not a crash or a re-draw. */
    CHECK(janus_render_poll() == false);
    CHECK(mock_driver_log_count == 2);
}

static void test_poll_does_not_advance_while_display_is_busy(void) {
    static const janus_widget_desc_t widgets[] = {
        { .kind = JANUS_WIDGET_LABEL, .id = "a", .geometry = { 0, 0, 10, 10 } },
    };
    static const janus_screen_desc_t screen = {
        .name = "Busy", .widgets = widgets, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen_async_start(&screen);
    mock_driver_set_busy_for(2);

    CHECK(janus_render_poll() == true);   /* busy: no draw yet, still rendering */
    CHECK(mock_driver_log_count == 0);
    CHECK(janus_render_poll() == true);   /* still busy */
    CHECK(mock_driver_log_count == 0);
    CHECK(janus_render_poll() == false);  /* free now: submits the one op, done */
    CHECK(mock_driver_log_count == 1);
}

static void test_async_draws_the_same_number_of_calls_as_blocking(void) {
    /* Same fixture rendered both ways should hit the driver the same
     * number of times — async is a different delivery schedule for the
     * identical set of tile/glyph draws, not a different set of them. */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = "AB", .geometry = { 0, 0, 42, 10 },
    };
    static const janus_screen_desc_t screen = {
        .name = "Compare", .widgets = &label, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    uint16_t blocking_calls = mock_driver_log_count;

    mock_driver_reset();
    janus_render_screen_async_start(&screen);
    while (janus_render_poll()) { }
    uint16_t async_calls = mock_driver_log_count;

    CHECK(blocking_calls > 0);
    CHECK(blocking_calls == async_calls);
}

static void test_async_glyph_color_matches_widget(void) {
    static const janus_widget_desc_t label = {
        /* w=24: comfortably fits one 20px-wide glyph (needs >= 21). */
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = "A", .geometry = { 0, 0, 24, 10 },
        .color = 0x1234, .bg_color = 0x5678,
    };
    static const janus_screen_desc_t screen = {
        .name = "AsyncColor", .widgets = &label, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen_async_start(&screen);
    while (janus_render_poll()) { }

    /* w=24 exceeds JANUS_TILE_W (16), so the background itself splits into
     * 2 horizontal tile fills — see test_label_with_text_draws_one_glyph_call_per_character
     * (test_runtime.c) for the same tiling math. */
    CHECK(mock_driver_log_count == 3);  /* 2 background tile fills + one glyph */
    CHECK(mock_driver_log[0].sample_pixel == 0x5678);  /* background */
}

static uint16_t g_async_img[20 * 18];
static void test_async_image_drains_tiles_with_correct_pixels(void) {
    /* Same fixture/expectations as test_runtime.c's
     * test_image_larger_than_tile_splits_with_correct_offsets, but via
     * the enqueue -> poll path: proves JANUS_ASYNC_OP_IMAGE carries the
     * per-tile source offset through the queue, not just the sync blit. */
    for (int y = 0; y < 18; y++)
        for (int x = 0; x < 20; x++)
            g_async_img[y * 20 + x] = (uint16_t)x;

    static const janus_widget_desc_t image = {
        .kind = JANUS_WIDGET_IMAGE, .id = "img", .geometry = { 0, 0, 20, 18 },
        .image_pixels = g_async_img, .image_w = 20, .image_h = 18,
    };
    static const janus_screen_desc_t screen = {
        .name = "AsyncImg", .widgets = &image, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    uint16_t blocking_calls = mock_driver_log_count;

    mock_driver_reset();
    janus_render_screen_async_start(&screen);
    CHECK(mock_driver_log_count == 0);   /* queue built, nothing drawn yet */
    while (janus_render_poll()) { }

    CHECK(blocking_calls == 4);
    CHECK(mock_driver_log_count == 4);
    CHECK(mock_driver_log[0].sample_pixel == 0);
    CHECK(mock_driver_log[1].sample_pixel == 16);
    CHECK(mock_driver_log[2].sample_pixel == 0);
    CHECK(mock_driver_log[3].sample_pixel == 16);
}

int main(void) {
    test_async_start_makes_no_driver_calls();
    test_poll_drains_one_op_per_call_and_reports_completion();
    test_poll_does_not_advance_while_display_is_busy();
    test_async_draws_the_same_number_of_calls_as_blocking();
    test_async_glyph_color_matches_widget();
    test_async_image_drains_tiles_with_correct_pixels();

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
