/* `focus_ring: true` rows (janus_focus_ring_t, fixes/000005). A widget
 * whose `.focus_ring` slot is set is ringed around its row's rect, not its
 * own; janus_set_focus draws that ring on focus and erases its band on
 * unfocus. Hand-built fixture shaped like Stage 2's output: each row's
 * children are inset by the ring width (6px), so the ring band is pixels
 * no child paints.
 *
 * Built twice (CMakeLists.txt): once against the plain library, where the
 * band is erased with the row's own bg, and once with
 * JANUS_DISPLAY_BACKGROUND forced, where it's erased with the canvas colour.
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

#define RING_SHADE ((uint16_t)0x0208)   /* JANUS_COLOR_FOCUS_SHADE */
#define RING_COLOR ((uint16_t)0x07ff)   /* JANUS_COLOR_FOCUS_RING — cyan */
#define ROW_BG     ((uint16_t)0x1234)

#if defined(JANUS_DISPLAY_BACKGROUND)
#define ERASE_COLOR ((uint16_t)JANUS_DISPLAY_BACKGROUND)
#else
#define ERASE_COLOR ROW_BG
#endif

static bool log_has(int16_t x, int16_t y, uint16_t colour) {
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        if (mock_driver_log[i].x == (uint16_t)x && mock_driver_log[i].y == (uint16_t)y &&
            mock_driver_log[i].sample_pixel == colour) return true;
    }
    return false;
}

/* The ring's outermost 1px line starts at the rect's own corner in the
 * shade colour; the next one in, at (x+1, y+1), in the bright cyan. */
static bool ring_drawn_at(janus_rect_t r) {
    return log_has(r.x, r.y, RING_SHADE) && log_has((int16_t)(r.x + 1), (int16_t)(r.y + 1), RING_COLOR);
}
static bool ring_erased_at(janus_rect_t r) {
    return log_has(r.x, r.y, ERASE_COLOR) && log_has((int16_t)(r.x + 1), (int16_t)(r.y + 1), ERASE_COLOR);
}

/* row_a (slot 1): toggle_a(0), button_a(1) — two focusables sharing a ring.
 * row_b (slot 2): toggle_b(2).
 * button_free(3): outside any ring row — rings itself, as before. */
static const janus_widget_desc_t row_a_children[] = {
    { .kind = JANUS_WIDGET_TOGGLE, .id = "toggle_a", .geometry = { 6, 6, 24, 12 },
      .action = 1, .navigate_target = -1, .focus_order = 0, .focus_ring = 1 },
    { .kind = JANUS_WIDGET_BUTTON, .id = "button_a", .geometry = { 34, 6, 60, 18 },
      .action = 2, .navigate_target = -1, .focus_order = 1, .focus_ring = 1 },
};
static const janus_widget_desc_t row_b_children[] = {
    { .kind = JANUS_WIDGET_TOGGLE, .id = "toggle_b", .geometry = { 6, 40, 24, 12 },
      .action = 3, .navigate_target = -1, .focus_order = 2, .focus_ring = 2 },
};
static const janus_widget_desc_t widgets[] = {
    { .kind = JANUS_WIDGET_ROW, .id = "row_a", .geometry = { 0, 0, 100, 30 },
      .bg_color = ROW_BG, .navigate_target = -1, .focus_order = JANUS_FOCUS_NONE,
      .children = row_a_children, .child_count = 2 },
    { .kind = JANUS_WIDGET_ROW, .id = "row_b", .geometry = { 0, 34, 100, 24 },
      .bg_color = ROW_BG, .navigate_target = -1, .focus_order = JANUS_FOCUS_NONE,
      .children = row_b_children, .child_count = 1 },
    { .kind = JANUS_WIDGET_BUTTON, .id = "button_free", .geometry = { 0, 70, 60, 20 },
      .action = 4, .navigate_target = -1, .focus_order = 3 },
};
static const janus_focus_ring_t rings[] = {
    { .rect = { 0, 0, 100, 30 }, .bg_color = ROW_BG },
    { .rect = { 0, 34, 100, 24 }, .bg_color = ROW_BG },
};
#define TOGGLE_A    (&row_a_children[0])
#define BUTTON_A    (&row_a_children[1])
#define TOGGLE_B    (&row_b_children[0])
#define BUTTON_FREE (&widgets[2])
static const janus_screen_desc_t screen = {
    .name = "Rings", .widgets = widgets, .widget_count = 3, .bound_struct = NULL,
    .focus_rings = rings,
};

static void reset(void) {
    mock_driver_reset();
    janus_render_screen(&screen);   /* sets g_current_screen */
    janus_set_focus(NULL);
    mock_driver_reset();
}

static void test_focusing_a_ring_row_widget_rings_the_whole_row(void) {
    reset();
    janus_set_focus(TOGGLE_A);
    CHECK(ring_drawn_at(rings[0].rect));
    /* not around the toggle's own (too small) rect */
    CHECK(!log_has(TOGGLE_A->geometry.x, TOGGLE_A->geometry.y, RING_SHADE));
}

static void test_a_ring_row_button_skips_its_own_ring(void) {
    reset();
    janus_set_focus(BUTTON_A);
    CHECK(ring_drawn_at(rings[0].rect));
    CHECK(!log_has(BUTTON_A->geometry.x, BUTTON_A->geometry.y, RING_SHADE));
}

static void test_moving_to_another_row_erases_the_old_ring(void) {
    reset();
    janus_set_focus(TOGGLE_A);
    mock_driver_reset();
    janus_set_focus(TOGGLE_B);
    CHECK(ring_erased_at(rings[0].rect));
    CHECK(ring_drawn_at(rings[1].rect));
}

static void test_moving_within_one_row_keeps_the_ring_up(void) {
    reset();
    janus_set_focus(TOGGLE_A);
    mock_driver_reset();
    janus_set_focus(BUTTON_A);
    CHECK(!log_has(rings[0].rect.x, rings[0].rect.y, ERASE_COLOR));   /* no erase -> no flicker */
    CHECK(ring_drawn_at(rings[0].rect));
}

static void test_clearing_focus_erases_the_ring(void) {
    reset();
    janus_set_focus(TOGGLE_B);
    mock_driver_reset();
    janus_set_focus(NULL);
    CHECK(ring_erased_at(rings[1].rect));
}

static void test_leaving_a_ring_row_for_a_plain_widget(void) {
    reset();
    janus_set_focus(TOGGLE_A);
    mock_driver_reset();
    janus_set_focus(BUTTON_FREE);
    CHECK(ring_erased_at(rings[0].rect));
    CHECK(ring_drawn_at(BUTTON_FREE->geometry));   /* unchanged: rings its own rect */
}

static void test_focus_move_reaches_ring_row_widgets(void) {
    static const janus_screen_desc_t *const screens[] = { &screen };
    static janus_app_t app = {
        .screens = screens, .screen_count = 1, .active_screen = 0,
        .nav_tabs = NULL, .nav_tab_count = 0, .nav_titles = NULL,
    };
    reset();
    janus_focus_move(&app, 0);   /* -> toggle_a, the first focusable */
    CHECK(janus_get_focus() == TOGGLE_A);
    CHECK(ring_drawn_at(rings[0].rect));
}

int main(void) {
    test_focusing_a_ring_row_widget_rings_the_whole_row();
    test_a_ring_row_button_skips_its_own_ring();
    test_moving_to_another_row_erases_the_old_ring();
    test_moving_within_one_row_keeps_the_ring_up();
    test_clearing_focus_erases_the_ring();
    test_leaving_a_ring_row_for_a_plain_widget();
    test_focus_move_reaches_ring_row_widgets();

    if (g_failures == 0) {
        printf("all focus ring tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d focus ring check(s) failed\n", g_failures);
    return 1;
}
