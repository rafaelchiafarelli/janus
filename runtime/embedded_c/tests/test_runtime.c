/* Stage 4 runtime library tests — hand-built janus_screen_desc_t
 * fixtures, no Python involved, asserted against the host mock driver's
 * call log. Plain checks + a pass/fail counter, no test framework
 * (matching this project's existing style for its Python tests).
 */
#include <stddef.h>
#include <stdio.h>

#include "janus_font.h"
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

/* ---- fixture 5: divider/toggle/badge/slider — the four "low effort"
 * kinds added on top of Stage 4/6, each reusing an existing bind shape. */
static void test_divider_always_draws_unconditionally(void) {
    static const janus_widget_desc_t divider = {
        .kind = JANUS_WIDGET_DIVIDER, .id = "d", .geometry = { 0, 0, 60, 2 },
    };
    static const janus_screen_desc_t screen = {
        .name = "Divider", .widgets = &divider, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count == 2); /* 60px wide spans two 32px tiles */
}

static void test_toggle_fill_tracks_live_value(void) {
    static const janus_widget_desc_t toggle = {
        .kind = JANUS_WIDGET_TOGGLE, .id = "t", .geometry = { 0, 0, 24, 12 },
        .bind = { .field_offset = offsetof(demo_t, level), .field_type = JANUS_FIELD_INT },
    };
    static const janus_screen_desc_t screen = {
        .name = "Toggle", .widgets = &toggle, .widget_count = 1, .bound_struct = &g_demo,
    };

    g_demo.level = 0;
    mock_driver_reset();
    janus_render_screen(&screen);
    uint8_t sample_off = mock_driver_log[0].sample_byte;

    g_demo.level = 1;
    mock_driver_reset();
    janus_render_screen(&screen);
    uint8_t sample_on = mock_driver_log[0].sample_byte;

    CHECK(sample_off != sample_on);
}

static void test_badge_fill_tracks_live_value(void) {
    static const janus_widget_desc_t badge = {
        .kind = JANUS_WIDGET_BADGE, .id = "b", .geometry = { 0, 0, 8, 8 },
        .bind = { .field_offset = offsetof(demo_t, level), .field_type = JANUS_FIELD_INT },
    };
    static const janus_screen_desc_t screen = {
        .name = "Badge", .widgets = &badge, .widget_count = 1, .bound_struct = &g_demo,
    };

    g_demo.level = 0;
    mock_driver_reset();
    janus_render_screen(&screen);
    uint8_t sample_off = mock_driver_log[0].sample_byte;

    g_demo.level = 1;
    mock_driver_reset();
    janus_render_screen(&screen);
    uint8_t sample_on = mock_driver_log[0].sample_byte;

    CHECK(sample_off != sample_on);
}

static void test_slider_fill_tracks_live_value(void) {
    static const janus_widget_desc_t slider = {
        .kind = JANUS_WIDGET_SLIDER, .id = "s", .geometry = { 0, 0, 100, 10 },
        .bind = {
            .field_offset = offsetof(demo_t, level), .field_type = JANUS_FIELD_INT,
            .range_min = 0, .range_max = 100,
        },
    };
    static const janus_screen_desc_t screen = {
        .name = "Slider", .widgets = &slider, .widget_count = 1, .bound_struct = &g_demo,
    };

    g_demo.level = 0;
    mock_driver_reset();
    janus_render_screen(&screen);
    uint8_t sample_at_0 = mock_driver_log[0].sample_byte;

    g_demo.level = 100;
    mock_driver_reset();
    janus_render_screen(&screen);
    uint8_t sample_at_100 = mock_driver_log[0].sample_byte;

    CHECK(sample_at_0 != sample_at_100);
}

/* ---- fixture 6: glyph rendering — authored static `text:` --- */

static int count_glyph_sized_calls(void) {
    int n = 0;
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        if (mock_driver_log[i].w == JANUS_FONT_GLYPH_W && mock_driver_log[i].h == JANUS_FONT_GLYPH_H) {
            n++;
        }
    }
    return n;
}

static void test_label_without_text_draws_only_the_background_fill(void) {
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = NULL, .geometry = { 0, 0, 10, 10 },
    };
    static const janus_screen_desc_t screen = {
        .name = "NoText", .widgets = &label, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count == 1);  /* the fill_rect only — no glyph calls */
    CHECK(count_glyph_sized_calls() == 0);
}

static void test_label_with_text_draws_one_glyph_call_per_character(void) {
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = "AB", .geometry = { 0, 0, 20, 10 },
    };
    static const janus_screen_desc_t screen = {
        .name = "Text", .widgets = &label, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count == 3);      /* 1 background fill + 2 glyphs */
    CHECK(count_glyph_sized_calls() == 2);
}

static void test_text_wider_than_widget_clips_without_wrapping(void) {
    /* Rect only fits one glyph column (w=6): 'A' draws, 'B' would start
     * past the right edge and must be dropped, not wrapped or squeezed. */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = "AB", .geometry = { 0, 0, 6, 10 },
    };
    static const janus_screen_desc_t screen = {
        .name = "Clipped", .widgets = &label, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(count_glyph_sized_calls() == 1);
}

/* ---- fixture 7: slice 2 — bound string fields (see architecture.md) --- */
typedef struct { const char *name; } demo_str_t;
static demo_str_t g_demo_str = { .name = NULL };

static void test_bound_string_with_value_draws_glyphs(void) {
    /* w=20 (< JANUS_TILE_W) so the background is exactly one fill call —
     * see test_divider_always_draws_unconditionally for why a wider rect
     * would split across tiles and change this count. */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = NULL, .geometry = { 0, 0, 20, 10 },
        .bind = { .field_offset = offsetof(demo_str_t, name), .field_type = JANUS_FIELD_STRING },
    };
    static const janus_screen_desc_t screen = {
        .name = "BoundString", .widgets = &label, .widget_count = 1, .bound_struct = &g_demo_str,
    };

    g_demo_str.name = "AB";
    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count == 3);      /* 1 background fill + 2 glyphs */
    CHECK(count_glyph_sized_calls() == 2);
}

static void test_bound_string_null_renders_fill_only(void) {
    /* Zero-initialized bindings instance (Stage 7: Janus generates shape,
     * not data) holds NULL until firmware populates it — must render as
     * the plain fill, not crash or draw garbage. */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = NULL, .geometry = { 0, 0, 20, 10 },
        .bind = { .field_offset = offsetof(demo_str_t, name), .field_type = JANUS_FIELD_STRING },
    };
    static const janus_screen_desc_t screen = {
        .name = "BoundStringNull", .widgets = &label, .widget_count = 1, .bound_struct = &g_demo_str,
    };

    g_demo_str.name = NULL;
    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count == 1);  /* the fill_rect only — no glyph calls */
    CHECK(count_glyph_sized_calls() == 0);
}

static void test_static_text_wins_over_bound_string(void) {
    /* Both set (nothing at parse time forbids it) — static_text is the
     * deterministic tie-break, see janus_runtime.c's draw_label comment. */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = "A", .geometry = { 0, 0, 60, 12 },
        .bind = { .field_offset = offsetof(demo_str_t, name), .field_type = JANUS_FIELD_STRING },
    };
    static const janus_screen_desc_t screen = {
        .name = "StaticWins", .widgets = &label, .widget_count = 1, .bound_struct = &g_demo_str,
    };

    g_demo_str.name = "ZZ";
    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(count_glyph_sized_calls() == 1);  /* "A", not "ZZ" */
}

static void test_box_header_draws_its_title_text(void) {
    static const janus_widget_desc_t box_widget = {
        .kind = JANUS_WIDGET_BOX, .id = "box1", .static_text = "AB",
        .geometry = { 0, 0, 30, 26 }, .geometry_collapsed = { 0, 0, 30, 16 },
        .initial_expanded = false,
    };
    static const janus_screen_desc_t screen = {
        .name = "BoxTitle", .widgets = &box_widget, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(count_glyph_sized_calls() == 2);
}

int main(void) {
    test_traversal_reaches_every_widget();
    test_progress_fill_tracks_live_value();
    test_box_collapse_and_toggle();
    test_switch_screen_draws_only_the_new_screen();
    test_divider_always_draws_unconditionally();
    test_toggle_fill_tracks_live_value();
    test_badge_fill_tracks_live_value();
    test_slider_fill_tracks_live_value();
    test_label_without_text_draws_only_the_background_fill();
    test_label_with_text_draws_one_glyph_call_per_character();
    test_text_wider_than_widget_clips_without_wrapping();
    test_bound_string_with_value_draws_glyphs();
    test_bound_string_null_renders_fill_only();
    test_static_text_wins_over_bound_string();
    test_box_header_draws_its_title_text();

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
