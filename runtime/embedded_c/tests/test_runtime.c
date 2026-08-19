/* Stage 4 runtime library tests — hand-built janus_screen_desc_t
 * fixtures, no Python involved, asserted against the host mock driver's
 * call log. Plain checks + a pass/fail counter, no test framework
 * (matching this project's existing style for its Python tests).
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

/* ---- fixture 1: traversal reaches every widget ---- */
static void test_traversal_reaches_every_widget(void) {
    static const janus_widget_desc_t widgets[] = {
        { .kind = JANUS_WIDGET_LABEL, .id = "a", .geometry = { 0, 0, 10, 10 } },
        { .kind = JANUS_WIDGET_LABEL, .id = "b", .geometry = { 0, 10, 10, 10 } },
    };
    static const janus_screen_desc_t screen = {
        .name = "Traversal", .widgets = widgets, .widget_count = 2, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count == 2);
}

/* ---- fixture 2: progress fill genuinely tracks the live bound value ---- */
typedef struct { int level; } demo_t;
static demo_t g_demo = { .level = 0 };

static void test_progress_fill_tracks_live_value(void) {
    static const janus_widget_desc_t progress = {
        .kind = JANUS_WIDGET_PROGRESS, .id = "p", .geometry = { 0, 0, 100, 10 },
        .bind = {
            .field_offset = offsetof(demo_t, level), .field_type = JANUS_FIELD_INT,
            .range_min = 0, .range_max = 100,
        },
    };
    static const janus_screen_desc_t screen = {
        .name = "Progress", .widgets = &progress, .widget_count = 1, .bound_struct = &g_demo,
    };

    g_demo.level = 0;
    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count > 0);
    uint8_t sample_at_0 = mock_driver_log[0].sample_byte;

    g_demo.level = 100;
    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count > 0);
    uint8_t sample_at_100 = mock_driver_log[0].sample_byte;

    CHECK(sample_at_0 != sample_at_100);
}

/* ---- fixture 3: box collapse/expand, real dual-geometry state ---- */
static void test_box_collapse_and_toggle(void) {
    static const janus_widget_desc_t box_child = {
        .kind = JANUS_WIDGET_LABEL, .id = "child", .geometry = { 0, 16, 10, 10 },
    };
    static const janus_widget_desc_t box_widget = {
        .kind = JANUS_WIDGET_BOX, .id = "box1",
        .geometry = { 0, 0, 10, 26 }, .geometry_collapsed = { 0, 0, 10, 16 },
        .initial_expanded = false,
        .children = &box_child, .child_count = 1,
    };
    static const janus_screen_desc_t screen = {
        .name = "Box", .widgets = &box_widget, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);          /* seeds box state as collapsed */
    CHECK(mock_driver_log_count == 1);     /* header only, child never drawn */

    mock_driver_reset();
    janus_toggle_box(&box_widget);         /* flips to expanded */
    CHECK(mock_driver_log_count == 2);     /* header + child */

    mock_driver_reset();
    janus_render_screen(&screen);          /* state persists across renders */
    CHECK(mock_driver_log_count == 2);
}

/* ---- fixture 4: only the active screen ever gets drawn ---- */
static void test_switch_screen_draws_only_the_new_screen(void) {
    static const janus_widget_desc_t s1_widget = {
        .kind = JANUS_WIDGET_LABEL, .id = "s1w", .geometry = { 0, 0, 10, 10 },
    };
    static const janus_widget_desc_t s2_widget = {
        .kind = JANUS_WIDGET_LABEL, .id = "s2w", .geometry = { 0, 0, 10, 10 },
    };
    static const janus_screen_desc_t s1 = { .name = "S1", .widgets = &s1_widget, .widget_count = 1 };
    static const janus_screen_desc_t s2 = { .name = "S2", .widgets = &s2_widget, .widget_count = 1 };
    static const janus_screen_desc_t *const screens[] = { &s1, &s2 };
    janus_app_t app = {
        .screens = screens, .nav_titles = NULL, .screen_count = 2, .active_screen = 0,
    };

    mock_driver_reset();
    janus_switch_screen(&app, 1);
    CHECK(app.active_screen == 1);
    CHECK(mock_driver_log_count == 1);     /* only s2's one widget, not s1's */
}

int main(void) {
    test_traversal_reaches_every_widget();
    test_progress_fill_tracks_live_value();
    test_box_collapse_and_toggle();
    test_switch_screen_draws_only_the_new_screen();

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
