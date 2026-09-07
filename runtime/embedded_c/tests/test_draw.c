/* janus_draw.c shape-primitive tests — janus_fill_rounded_rect /
 * janus_fill_circle. Called directly (they're internal-but-extern, see
 * janus_draw.h) and asserted against the host mock driver's call log:
 * each helper decomposes to fill_rect, and a small shape's every span is
 * <= 16px so it lands as exactly one draw call, which keeps the
 * coordinate maths checkable pixel-by-pixel.
 *
 * The async-path assertion the task originally sketched here (a widget
 * whose draw calls these under janus_render_screen_async_start) moves to
 * ui_widgets/kind_visuals task 1 (toggle), where a real widget actually
 * exercises them through the queued path — nothing in the runtime calls
 * these primitives yet, and g_async_enqueue is private to
 * janus_runtime.c. Async-safety is structural regardless: the only
 * driver-touching call underneath is fill_rect, already covered by
 * test_render_async.c.
 */
#include <stdio.h>

#include "janus_draw.h"
#include "mock_driver.h"

static int g_failures = 0;

#define CHECK(cond) do { \
    if (!(cond)) { \
        g_failures++; \
        fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
    } \
} while (0)

static int covers(int px, int py) {
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        mock_draw_call_t c = mock_driver_log[i];
        if ((uint16_t)px >= c.x && (uint16_t)px < c.x + c.w &&
            (uint16_t)py >= c.y && (uint16_t)py < c.y + c.h) return 1;
    }
    return 0;
}

static int covers_colour(int px, int py, uint16_t col) {
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        mock_draw_call_t c = mock_driver_log[i];
        if ((uint16_t)px >= c.x && (uint16_t)px < c.x + c.w &&
            (uint16_t)py >= c.y && (uint16_t)py < c.y + c.h && c.sample_pixel == col) return 1;
    }
    return 0;
}

static void test_rounded_rect_radius_zero_is_a_plain_fill(void) {
    mock_driver_reset();
    janus_fill_rounded_rect((janus_rect_t){ 0, 0, 10, 10 }, 0, 0xABCD);
    /* 10x10 is a single sub-tile fill -> exactly one draw call, the whole rect */
    CHECK(mock_driver_log_count == 1);
    CHECK(mock_driver_log[0].x == 0 && mock_driver_log[0].y == 0);
    CHECK(mock_driver_log[0].w == 10 && mock_driver_log[0].h == 10);
    CHECK(mock_driver_log[0].sample_pixel == 0xABCD);
}

static void test_rounded_rect_corner_is_clipped_off(void) {
    mock_driver_reset();
    janus_fill_rounded_rect((janus_rect_t){ 0, 0, 20, 20 }, 4, 0x1111);
    /* the true corner pixel is outside the rounding */
    CHECK(!covers(0, 0));
    CHECK(!covers(19, 0));
    CHECK(!covers(0, 19));
    /* ...but the edge midpoints and a point `radius` in along the top are in */
    CHECK(covers_colour(4, 0, 0x1111));   /* (x + radius, y) */
    CHECK(covers_colour(0, 10, 0x1111));  /* left edge, vertical middle */
    CHECK(covers_colour(10, 10, 0x1111)); /* dead centre */
}

static void test_rounded_rect_emits_2r_plus_1_spans_worth_of_fills(void) {
    /* w,h chosen so every span is <= 16px: 1 centre band + 2*radius rows. */
    mock_driver_reset();
    janus_fill_rounded_rect((janus_rect_t){ 0, 0, 12, 12 }, 3, 0x2222);
    CHECK(mock_driver_log_count == (uint16_t)(2 * 3 + 1));
}

static void test_circle_centre_and_extent(void) {
    mock_driver_reset();
    janus_fill_circle(20, 20, 5, 0x3333);
    CHECK(covers_colour(20, 20, 0x3333));  /* centre */
    CHECK(covers_colour(25, 20, 0x3333));  /* rightmost pixel on the centre row */
    CHECK(!covers(26, 20));                /* one past it */
    CHECK(covers_colour(20, 15, 0x3333));  /* top pixel (cy - r) */
    CHECK(!covers(20, 14));
    CHECK(mock_driver_log_count == (uint16_t)(2 * 5 + 1));
}

static void test_circle_no_op_for_non_positive_radius(void) {
    mock_driver_reset();
    janus_fill_circle(10, 10, 0, 0x4444);
    janus_fill_circle(10, 10, -3, 0x4444);
    CHECK(mock_driver_log_count == 0);
}

static void test_circle_straddling_x_zero_writes_no_negative_x(void) {
    mock_driver_reset();
    janus_fill_circle(3, 20, 5, 0x5555);   /* leftmost span would start at x = -2 */
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        CHECK(mock_driver_log[i].x < 0x8000);   /* no wrapped-negative origin */
    }
    CHECK(covers_colour(0, 20, 0x5555));   /* the clipped span still starts at x = 0 */
    CHECK(covers_colour(8, 20, 0x5555));   /* right extent unchanged: cx + r = 8 */
    CHECK(!covers(9, 20));                  /* one past it */
}

/* ---- task 2: shade / line / trig ---- */

static int r5(uint16_t px) { return (px >> 11) & 0x1F; }

static void test_rgb565_lerp_endpoints_and_identity(void) {
    CHECK(janus_rgb565_lerp(0x0000, 0xFFFF, 0)   == 0x0000);
    CHECK(janus_rgb565_lerp(0x0000, 0xFFFF, 255) == 0xFFFF);
    CHECK(janus_rgb565_lerp(0x1234, 0x1234, 128) == 0x1234);
}

static void test_shade_rect_v_runs_top_to_bottom_monotone(void) {
    mock_driver_reset();
    janus_shade_rect_v((janus_rect_t){ 0, 0, 1, 10 }, 0x0000, 0xF800);
    CHECK(mock_driver_log_count == 10);
    CHECK(mock_driver_log[0].sample_pixel == 0x0000);
    CHECK(mock_driver_log[9].sample_pixel == 0xF800);
    for (uint16_t i = 1; i < mock_driver_log_count; i++) {
        CHECK(r5(mock_driver_log[i].sample_pixel) >= r5(mock_driver_log[i - 1].sample_pixel));
    }
}

static void test_shade_rect_v_single_row_is_a_flat_top_fill(void) {
    mock_driver_reset();
    janus_shade_rect_v((janus_rect_t){ 0, 0, 4, 1 }, 0x07E0, 0xF800);
    CHECK(mock_driver_log_count == 1);
    CHECK(mock_driver_log[0].sample_pixel == 0x07E0);
}

static void test_draw_line_vertical_is_contiguous_on_one_column(void) {
    mock_driver_reset();
    janus_draw_line(2, 2, 2, 8, 0x9999);
    CHECK(mock_driver_log_count == 7);   /* (2,2)..(2,8) inclusive */
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        CHECK(mock_driver_log[i].x == 2);
        CHECK(mock_driver_log[i].w == 1 && mock_driver_log[i].h == 1);
    }
    CHECK(covers(2, 2));
    CHECK(covers(2, 8));
    CHECK(!covers(2, 9));
}

static void test_draw_line_diagonal_hits_both_endpoints(void) {
    mock_driver_reset();
    janus_draw_line(0, 0, 5, 5, 0xAAAA);
    CHECK(covers_colour(0, 0, 0xAAAA));
    CHECK(covers_colour(5, 5, 0xAAAA));
    CHECK(mock_driver_log_count == 6);
}

static void test_sin16_cos16_known_angles(void) {
    CHECK(janus_sin16(0) == 0);
    CHECK(janus_sin16(90) == 32767);
    CHECK(janus_sin16(180) == 0);
    CHECK(janus_sin16(270) == -32767);
    CHECK(janus_sin16(-90) == -32767);
    int16_t s30 = janus_sin16(30);
    CHECK(s30 >= 16383 && s30 <= 16385);   /* ~0.5 * 32767 */
    CHECK(janus_cos16(0) == 32767);
    CHECK(janus_cos16(90) == 0);
    CHECK(janus_cos16(180) == -32767);
    /* full-turn reduction */
    CHECK(janus_sin16(360) == janus_sin16(0));
    CHECK(janus_sin16(450) == janus_sin16(90));
}

int main(void) {
    test_rounded_rect_radius_zero_is_a_plain_fill();
    test_rounded_rect_corner_is_clipped_off();
    test_rounded_rect_emits_2r_plus_1_spans_worth_of_fills();
    test_circle_centre_and_extent();
    test_circle_no_op_for_non_positive_radius();
    test_circle_straddling_x_zero_writes_no_negative_x();
    test_rgb565_lerp_endpoints_and_identity();
    test_shade_rect_v_runs_top_to_bottom_monotone();
    test_shade_rect_v_single_row_is_a_flat_top_fill();
    test_draw_line_vertical_is_contiguous_on_one_column();
    test_draw_line_diagonal_hits_both_endpoints();
    test_sin16_cos16_known_angles();

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
