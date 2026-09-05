/* Stage 4 runtime library tests — hand-built janus_screen_desc_t
 * fixtures, no Python involved, asserted against the host mock driver's
 * call log. Plain checks + a pass/fail counter, no test framework
 * (matching this project's existing style for its Python tests).
 *
 * Tile size is JANUS_TILE_W x JANUS_TILE_H = 16x16 (janus_runtime.c) —
 * fixtures that assert an exact mock_driver_log_count keep their
 * rectangles under 16px on the axis that matters so a fill doesn't
 * silently split across tiles and change the expected count.
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
        .color = 0x1111, .bg_color = 0xeeee,
    };
    static const janus_screen_desc_t screen = {
        .name = "Progress", .widgets = &progress, .widget_count = 1, .bound_struct = &g_demo,
    };

    g_demo.level = 0;
    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count > 0);
    uint16_t sample_at_0 = mock_driver_log[0].sample_pixel;

    g_demo.level = 100;
    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count > 0);
    uint16_t sample_at_100 = mock_driver_log[0].sample_pixel;

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
    /* clear (2 tiles: {0,0,10,26} splits at the 16px tile boundary) +
     * header (1) + child (1) — the clear is the vacated-body-area fix
     * (fixture 3a below), not specific to this fixture's own concern. */
    CHECK(mock_driver_log_count == 4);

    mock_driver_reset();
    janus_render_screen(&screen);          /* state persists across renders */
    CHECK(mock_driver_log_count == 2);
}

/* ---- fixture 2b: janus_render_widget draws exactly one widget, not the
 * whole screen — the entry point for redrawing a known subtree (e.g. a
 * header) on its own cadence. ---- */
static void test_render_widget_draws_only_that_widget(void) {
    static const janus_widget_desc_t widgets[] = {
        { .kind = JANUS_WIDGET_LABEL, .id = "a", .geometry = { 0, 0, 10, 10 } },
        { .kind = JANUS_WIDGET_LABEL, .id = "b", .geometry = { 0, 10, 10, 10 } },
    };

    mock_driver_reset();
    janus_render_widget(&widgets[1], NULL);
    CHECK(mock_driver_log_count == 1);
    CHECK(mock_driver_log[0].y == 10);   /* widgets[1], not widgets[0] */
}

/* ---- fixture 2c: dirty-aware rendering — a bound leaf only redraws when
 * firmware has actually marked its field's bit dirty; the bit is
 * consumed (cleared) on that redraw. Unbound (static) leaves always
 * draw regardless — nothing ever marks them dirty, by design. ---- */
typedef struct { int level; } dirty_demo_t;
typedef struct { bool level; } dirty_demo_dirty_t;

static void test_render_widget_if_dirty_skips_unchanged_field(void) {
    static dirty_demo_t data = { .level = 5 };
    static dirty_demo_dirty_t dirty = { .level = false };
    static const janus_widget_desc_t w = {
        .kind = JANUS_WIDGET_LABEL, .id = "p", .geometry = { 0, 0, 10, 10 },
        .bind = {
            .field_offset = offsetof(dirty_demo_t, level),
            .dirty_offset = offsetof(dirty_demo_dirty_t, level),
            .field_type = JANUS_FIELD_INT,
        },
    };

    mock_driver_reset();
    janus_render_widget_if_dirty(&w, &data, &dirty);
    CHECK(mock_driver_log_count == 0);   /* not dirty -- skipped entirely */

    dirty.level = true;
    mock_driver_reset();
    janus_render_widget_if_dirty(&w, &data, &dirty);
    CHECK(mock_driver_log_count == 1);   /* dirty -- drawn */
    CHECK(dirty.level == false);         /* ...and the bit is consumed */

    mock_driver_reset();
    janus_render_widget_if_dirty(&w, &data, &dirty);
    CHECK(mock_driver_log_count == 0);   /* clean again -- skipped */
}

static void test_unbound_widget_always_draws_via_if_dirty(void) {
    static const janus_widget_desc_t w = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = "hi", .geometry = { 0, 0, 20, 10 },
    };

    mock_driver_reset();
    janus_render_widget_if_dirty(&w, NULL, NULL);
    CHECK(mock_driver_log_count > 0);

    mock_driver_reset();
    janus_render_widget_if_dirty(&w, NULL, NULL);
    CHECK(mock_driver_log_count > 0);    /* still draws every time -- nothing ever marks it clean */
}

static void test_render_screen_if_dirty_respects_the_bit(void) {
    static dirty_demo_t data = { .level = 5 };
    static dirty_demo_dirty_t dirty = { .level = false };
    static const janus_widget_desc_t widget = {
        .kind = JANUS_WIDGET_LABEL, .id = "p", .geometry = { 0, 0, 10, 10 },
        .bind = {
            .field_offset = offsetof(dirty_demo_t, level),
            .dirty_offset = offsetof(dirty_demo_dirty_t, level),
            .field_type = JANUS_FIELD_INT,
        },
    };
    static const janus_screen_desc_t screen = {
        .name = "Dirty", .widgets = &widget, .widget_count = 1,
        .bound_struct = &data, .bound_dirty = &dirty,
    };

    mock_driver_reset();
    janus_render_screen_if_dirty(&screen);
    CHECK(mock_driver_log_count == 0);

    dirty.level = true;
    mock_driver_reset();
    janus_render_screen_if_dirty(&screen);
    CHECK(mock_driver_log_count == 1);

    /* janus_render_screen (the non-dirty-aware entry point) is unaffected
     * -- always forces a real redraw regardless of the bit's state. */
    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count == 1);
}

/* ---- fixture 3a: collapsing a box must clear the vacated body area, not
 * just repaint the header — otherwise the previous render's child pixels
 * (drawn below the header, now no longer part of the collapsed layout)
 * stay on screen forever, since nothing else ever repaints a rect a
 * widget doesn't currently own. Regression: found independently on real
 * ArduinoIHM hardware before this fix was ported back here. ---- */
static void test_toggle_box_clears_vacated_body_on_collapse(void) {
    static const janus_widget_desc_t box_child = {
        .kind = JANUS_WIDGET_LABEL, .id = "child", .geometry = { 0, 16, 10, 10 },
    };
    static const janus_widget_desc_t box_widget = {
        .kind = JANUS_WIDGET_BOX, .id = "box1",
        .geometry = { 0, 0, 10, 26 }, .geometry_collapsed = { 0, 0, 10, 16 },
        .initial_expanded = true,
        .children = &box_child, .child_count = 1,
    };
    static const janus_screen_desc_t screen = {
        .name = "BoxCollapse", .widgets = &box_widget, .widget_count = 1, .bound_struct = NULL,
    };

    janus_render_screen(&screen);          /* seeds box state as expanded */

    mock_driver_reset();
    janus_toggle_box(&box_widget);         /* flips to collapsed */

    uint16_t max_bottom = 0;
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        uint16_t bottom = mock_driver_log[i].y + mock_driver_log[i].h;
        if (bottom > max_bottom) max_bottom = bottom;
    }
    CHECK(max_bottom >= 26);   /* clears past the 16px header into the vacated body */
}

/* ---- fixture 3b: box.summary — always drawn, collapsed or expanded,
 * unlike .children (expanded-only, see fixture 3 above) ---- */
static void test_box_summary_renders_when_collapsed_and_expanded(void) {
    static const janus_widget_desc_t summary_led = {
        .kind = JANUS_WIDGET_LED, .id = "status_led", .geometry = { 0, 0, 10, 10 },
    };
    static const janus_widget_desc_t box_child = {
        .kind = JANUS_WIDGET_LABEL, .id = "child", .geometry = { 0, 16, 10, 10 },
    };
    static const janus_widget_desc_t box_widget = {
        .kind = JANUS_WIDGET_BOX, .id = "box1",
        .geometry = { 0, 0, 10, 26 }, .geometry_collapsed = { 0, 0, 10, 16 },
        .initial_expanded = false,
        .children = &box_child, .child_count = 1,
        .summary_children = &summary_led, .summary_child_count = 1,
    };
    static const janus_screen_desc_t screen = {
        .name = "BoxSummary", .widgets = &box_widget, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);          /* collapsed: header + summary, no detail child */
    CHECK(mock_driver_log_count == 2);

    mock_driver_reset();
    janus_toggle_box(&box_widget);         /* expanded: header + summary + detail child */
    /* + the 2-tile vacated-body clear (fixture 3a) ahead of header/summary/child */
    CHECK(mock_driver_log_count == 5);
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
    CHECK(mock_driver_log_count == 4); /* 60px wide spans four 16px tiles (16+16+16+12) */
}

static void test_toggle_fill_tracks_live_value(void) {
    static const janus_widget_desc_t toggle = {
        .kind = JANUS_WIDGET_TOGGLE, .id = "t", .geometry = { 0, 0, 12, 12 },
        .bind = { .field_offset = offsetof(demo_t, level), .field_type = JANUS_FIELD_INT },
        .color = 0x2222, .bg_color = 0xdddd,
    };
    static const janus_screen_desc_t screen = {
        .name = "Toggle", .widgets = &toggle, .widget_count = 1, .bound_struct = &g_demo,
    };

    g_demo.level = 0;
    mock_driver_reset();
    janus_render_screen(&screen);
    uint16_t sample_off = mock_driver_log[0].sample_pixel;

    g_demo.level = 1;
    mock_driver_reset();
    janus_render_screen(&screen);
    uint16_t sample_on = mock_driver_log[0].sample_pixel;

    CHECK(sample_off != sample_on);
}

static void test_badge_fill_tracks_live_value(void) {
    static const janus_widget_desc_t badge = {
        .kind = JANUS_WIDGET_BADGE, .id = "b", .geometry = { 0, 0, 8, 8 },
        .bind = { .field_offset = offsetof(demo_t, level), .field_type = JANUS_FIELD_INT },
        .color = 0x3333, .bg_color = 0xcccc,
    };
    static const janus_screen_desc_t screen = {
        .name = "Badge", .widgets = &badge, .widget_count = 1, .bound_struct = &g_demo,
    };

    g_demo.level = 0;
    mock_driver_reset();
    janus_render_screen(&screen);
    uint16_t sample_off = mock_driver_log[0].sample_pixel;

    g_demo.level = 1;
    mock_driver_reset();
    janus_render_screen(&screen);
    uint16_t sample_on = mock_driver_log[0].sample_pixel;

    CHECK(sample_off != sample_on);
}

static void test_slider_fill_tracks_live_value(void) {
    static const janus_widget_desc_t slider = {
        .kind = JANUS_WIDGET_SLIDER, .id = "s", .geometry = { 0, 0, 100, 10 },
        .bind = {
            .field_offset = offsetof(demo_t, level), .field_type = JANUS_FIELD_INT,
            .range_min = 0, .range_max = 100,
        },
        .color = 0x4444, .bg_color = 0xbbbb,
    };
    static const janus_screen_desc_t screen = {
        .name = "Slider", .widgets = &slider, .widget_count = 1, .bound_struct = &g_demo,
    };

    g_demo.level = 0;
    mock_driver_reset();
    janus_render_screen(&screen);
    uint16_t sample_at_0 = mock_driver_log[0].sample_pixel;

    g_demo.level = 100;
    mock_driver_reset();
    janus_render_screen(&screen);
    uint16_t sample_at_100 = mock_driver_log[0].sample_pixel;

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
    /* w=14 (< JANUS_TILE_W) so the background is exactly one fill call —
     * see test_divider_always_draws_unconditionally for why a wider rect
     * would split across tiles and change this count. */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = "AB", .geometry = { 0, 0, 14, 10 },
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
    /* w=14 (< JANUS_TILE_W) so the background is exactly one fill call —
     * see test_divider_always_draws_unconditionally for why a wider rect
     * would split across tiles and change this count. */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = NULL, .geometry = { 0, 0, 14, 10 },
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
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = NULL, .geometry = { 0, 0, 14, 10 },
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

/* ---- fixture 8: authored RGB565 color actually reaches the driver ---- */
static void test_widget_authored_color_is_what_gets_drawn(void) {
    static const janus_widget_desc_t image = {
        .kind = JANUS_WIDGET_IMAGE, .id = "i", .geometry = { 0, 0, 10, 10 },
        .color = 0xf800, /* pure red */
    };
    static const janus_screen_desc_t screen = {
        .name = "Colored", .widgets = &image, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count == 1);
    CHECK(mock_driver_log[0].sample_pixel == 0xf800);
}

static void test_widget_default_colors_are_the_runtime_constants(void) {
    /* No .color/.bg_color authored — a zero-initialized struct literal
     * leaves both at 0, which is JANUS_COLOR_DEFAULT_FG's own value, so
     * this only proves something meaningful once compared against a
     * widget that explicitly authors JANUS_COLOR_DEFAULT_BG for bg. */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = NULL, .geometry = { 0, 0, 10, 10 },
        .color = JANUS_COLOR_DEFAULT_FG, .bg_color = JANUS_COLOR_DEFAULT_BG,
    };
    static const janus_screen_desc_t screen = {
        .name = "Default", .widgets = &label, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count == 1);
    CHECK(mock_driver_log[0].sample_pixel == JANUS_COLOR_DEFAULT_BG);
}

int main(void) {
    test_traversal_reaches_every_widget();
    test_progress_fill_tracks_live_value();
    test_render_widget_draws_only_that_widget();
    test_render_widget_if_dirty_skips_unchanged_field();
    test_unbound_widget_always_draws_via_if_dirty();
    test_render_screen_if_dirty_respects_the_bit();
    test_box_collapse_and_toggle();
    test_toggle_box_clears_vacated_body_on_collapse();
    test_box_summary_renders_when_collapsed_and_expanded();
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
    test_widget_authored_color_is_what_gets_drawn();
    test_widget_default_colors_are_the_runtime_constants();

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
