/* Stage 4 runtime library tests — hand-built janus_screen_desc_t
 * fixtures, no Python involved, asserted against the host mock driver's
 * call log. Plain checks + a pass/fail counter, no test framework
 * (matching this project's existing style for its Python tests).
 *
 * Tile size is JANUS_TILE_W x JANUS_TILE_H = 16x16 (janus_runtime.c) —
 * fixtures that assert an exact mock_driver_log_count keep their
 * rectangles under 16px on the axis that matters so a fill doesn't
 * silently split across tiles and change the expected count. The
 * text-drawing fixtures are the exception: every widget desc below is
 * zero-initialized for `.font_size`/`.font_scale`, which default to
 * `large` at scale 1 (janus_font.h) — JANUS_FONT_LARGE_GLYPH_W (20) alone
 * forces any rect wide enough to hold more than one glyph past 16px, so
 * those fixtures' expected counts account for the resulting tile split
 * instead of avoiding it.
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
        if (mock_driver_log[i].w == JANUS_FONT_LARGE_GLYPH_W && mock_driver_log[i].h == JANUS_FONT_LARGE_GLYPH_H) {
            n++;
        }
    }
    return n;
}

static int count_calls_of_size(uint16_t w, uint16_t h) {
    int n = 0;
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        if (mock_driver_log[i].w == w && mock_driver_log[i].h == h) n++;
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
    /* w=42 is the minimum that fits both 20px-wide glyphs (1px left pad +
     * 20 + 1px gap + 20 = 42) — wider than JANUS_TILE_W (16), so the
     * background itself now splits across 3 horizontal tiles (16+16+10);
     * see test_divider_always_draws_unconditionally for that tiling math. */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = "AB", .geometry = { 0, 0, 42, 10 },
    };
    static const janus_screen_desc_t screen = {
        .name = "Text", .widgets = &label, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count == 5);      /* 3 background tile fills + 2 glyphs */
    CHECK(count_glyph_sized_calls() == 2);
}

static void test_text_wider_than_widget_clips_without_wrapping(void) {
    /* Rect only fits one glyph column (w=21 = 1px left pad + 20): 'A'
     * draws, 'B' would start past the right edge and must be dropped, not
     * wrapped or squeezed. */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = "AB", .geometry = { 0, 0, 21, 10 },
    };
    static const janus_screen_desc_t screen = {
        .name = "Clipped", .widgets = &label, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(count_glyph_sized_calls() == 1);
}

/* ---- fixture 6b: font_size/font_scale (2026-09-05: medium/large tables +
 * scale, see janus_font.h) --- */

static void test_medium_font_size_draws_medium_sized_glyph(void) {
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = "A", .geometry = { 0, 0, 14, 16 },
        .font_size = JANUS_FONT_SIZE_MEDIUM,
    };
    static const janus_screen_desc_t screen = {
        .name = "Medium", .widgets = &label, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(count_calls_of_size(JANUS_FONT_MEDIUM_GLYPH_W, JANUS_FONT_MEDIUM_GLYPH_H) == 1);
    CHECK(count_glyph_sized_calls() == 0);   /* not drawn at the large size */
}

static void test_font_scale_multiplies_medium_up_to_large_footprint(void) {
    /* 2x medium (10x14) == large's own native size (20x28) — the exact
     * relationship janus_font.h documents. */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = "A", .geometry = { 0, 0, 24, 30 },
        .font_size = JANUS_FONT_SIZE_MEDIUM, .font_scale = 2,
    };
    static const janus_screen_desc_t screen = {
        .name = "MediumScaled", .widgets = &label, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(count_glyph_sized_calls() == 1);   /* JANUS_FONT_LARGE_GLYPH_W/H, i.e. 20x28 */
}

static void test_font_scale_beyond_the_cap_is_clamped_not_overflowed(void) {
    /* .font_scale = 5 on `large` would need a 100x140 glyph (14000 px) --
     * draw_string must clamp this back to whatever fits g_tile_buffer
     * (JANUS_TILE_BUFFER_PIXELS, sized for large's own 20x28) instead of
     * overrunning it. Geometry is generous (200 wide) so clipping isn't
     * what limits the drawn size here -- only the clamp is. */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = "A", .geometry = { 0, 0, 200, 150 },
        .font_size = JANUS_FONT_SIZE_LARGE, .font_scale = 5,
    };
    static const janus_screen_desc_t screen = {
        .name = "OverScaled", .widgets = &label, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(count_calls_of_size(100, 140) == 0);   /* the naive, unclamped 5x size */
    CHECK(count_glyph_sized_calls() == 1);       /* clamped back to large's native 20x28 */
}

/* ---- fixture 7: slice 2 — bound string fields (see architecture.md) --- */
typedef struct { const char *name; } demo_str_t;
static demo_str_t g_demo_str = { .name = NULL };

static void test_bound_string_with_value_draws_glyphs(void) {
    /* w=42 is the minimum that fits both 20px-wide glyphs — see
     * test_label_with_text_draws_one_glyph_call_per_character above for
     * the exact tiling math this implies for the background fill. */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = NULL, .geometry = { 0, 0, 42, 10 },
        .bind = { .field_offset = offsetof(demo_str_t, name), .field_type = JANUS_FIELD_STRING },
    };
    static const janus_screen_desc_t screen = {
        .name = "BoundString", .widgets = &label, .widget_count = 1, .bound_struct = &g_demo_str,
    };

    g_demo_str.name = "AB";
    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count == 5);      /* 3 background tile fills + 2 glyphs */
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
    /* geometry_collapsed's width is what draw_box_header uses for the
     * title (janus_runtime.c) — 42 is the minimum that fits both
     * 20px-wide glyphs, same math as the label fixtures above. */
    static const janus_widget_desc_t box_widget = {
        .kind = JANUS_WIDGET_BOX, .id = "box1", .static_text = "AB",
        .geometry = { 0, 0, 42, 26 }, .geometry_collapsed = { 0, 0, 42, 16 },
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

/* ---- fixture 9: image widget blits its baked RGB565 pixels ----
 * The pixel data + the per-screen far-address table + resolver mirror
 * what emit_screen generates: the descriptor carries a 1-based slot, the
 * runtime calls resolve_images() on screen-enter to fill the table, blit
 * reads through JANUS_MEMCPY_PF. */
static const uint16_t g_img_pixels[16] = {
    0x1234, 1, 2, 3,
    4, 5, 6, 7,
    8, 9, 10, 11,
    12, 13, 14, 15,
};
static janus_farptr_t g_img_far[1];
static void resolve_img(void) { g_img_far[0] = JANUS_FAR_ADDR(g_img_pixels); }

static void test_image_widget_blits_its_pixels(void) {
    /* 4x4, top-left pixel a known value — geometry matches the image, so
     * one draw call, and its sample_pixel is that top-left. */
    static const janus_widget_desc_t image = {
        .kind = JANUS_WIDGET_IMAGE, .id = "img", .geometry = { 0, 0, 4, 4 },
        .color = 0xf800, /* would show if the stub fill path ran instead */
        .image_slot = 1, .image_w = 4, .image_h = 4,
    };
    static const janus_screen_desc_t screen = {
        .name = "Img", .widgets = &image, .widget_count = 1, .bound_struct = NULL,
        .resolve_images = resolve_img, .image_far = g_img_far,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count == 1);
    CHECK(mock_driver_log[0].w == 4 && mock_driver_log[0].h == 4);
    CHECK(mock_driver_log[0].sample_pixel == 0x1234);
}

/* ---- fixture 9b: a `file:` that wouldn't decode -> magenta placeholder ---- */
static void test_image_error_paints_magenta(void) {
    static const janus_widget_desc_t image = {
        .kind = JANUS_WIDGET_IMAGE, .id = "bad", .geometry = { 0, 0, 10, 10 },
        .color = 0x07e0, /* green — must NOT be what draws */
        .image_error = true,
    };
    static const janus_screen_desc_t screen = {
        .name = "Bad", .widgets = &image, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(mock_driver_log_count == 1);
    CHECK(mock_driver_log[0].sample_pixel == 0xf81f);   /* JANUS_COLOR_IMAGE_MISSING */
}

/* ---- fixture 9c: an image bigger than one tile splits, per-tile source
 * offset stays correct (pixel value == column index here) ---- */
static uint16_t g_big_pixels[20 * 18];
static janus_farptr_t g_big_far[1];
static void resolve_big(void) { g_big_far[0] = JANUS_FAR_ADDR(g_big_pixels); }
static void test_image_larger_than_tile_splits_with_correct_offsets(void) {
    for (int y = 0; y < 18; y++)
        for (int x = 0; x < 20; x++)
            g_big_pixels[y * 20 + x] = (uint16_t)x;

    static const janus_widget_desc_t image = {
        .kind = JANUS_WIDGET_IMAGE, .id = "big", .geometry = { 0, 0, 20, 18 },
        .image_slot = 1, .image_w = 20, .image_h = 18,
    };
    static const janus_screen_desc_t screen = {
        .name = "Big", .widgets = &image, .widget_count = 1, .bound_struct = NULL,
        .resolve_images = resolve_big, .image_far = g_big_far,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    /* 20x18 over 16x16 tiles -> 2 cols x 2 rows = 4 calls, row-major */
    CHECK(mock_driver_log_count == 4);
    CHECK(mock_driver_log[0].sample_pixel == 0);    /* tile (0,0)  -> col 0  */
    CHECK(mock_driver_log[1].sample_pixel == 16);   /* tile (16,0) -> col 16 */
    CHECK(mock_driver_log[2].sample_pixel == 0);    /* tile (0,16) -> col 0  */
    CHECK(mock_driver_log[3].sample_pixel == 16);   /* tile (16,16)-> col 16 */
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
    test_medium_font_size_draws_medium_sized_glyph();
    test_font_scale_multiplies_medium_up_to_large_footprint();
    test_font_scale_beyond_the_cap_is_clamped_not_overflowed();
    test_bound_string_with_value_draws_glyphs();
    test_bound_string_null_renders_fill_only();
    test_static_text_wins_over_bound_string();
    test_box_header_draws_its_title_text();
    test_widget_authored_color_is_what_gets_drawn();
    test_widget_default_colors_are_the_runtime_constants();
    test_image_widget_blits_its_pixels();
    test_image_error_paints_magenta();
    test_image_larger_than_tile_splits_with_correct_offsets();

    if (g_failures == 0) {
        printf("all tests passed\n");
        return 0;
    }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
