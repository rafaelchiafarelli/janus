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

#include "janus_draw.h"
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

/* ---- shared shape-render assertions (kind_visuals) ------------------ *
 * The toggle/progress/led renders decompose to many fill_rect spans, so
 * these scan the mock log by geometry / colour rather than by call
 * count. Each span is a uniform fill, so `sample_pixel` is its colour. */

/* was any pixel (px,py) painted at all, by any logged span? */
static int rt_painted(int px, int py) {
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        mock_draw_call_t c = mock_driver_log[i];
        if ((int)c.x <= px && px < (int)(c.x + c.w) &&
            (int)c.y <= py && py < (int)(c.y + c.h)) return 1;
    }
    return 0;
}

/* was (px,py) painted `colour` by some span (ignores later overpaint —
 * "a colour pixel exists here", which is what the task assertions want) */
static int rt_painted_colour(int px, int py, uint16_t colour) {
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        mock_draw_call_t c = mock_driver_log[i];
        if (c.sample_pixel != colour) continue;
        if ((int)c.x <= px && px < (int)(c.x + c.w) &&
            (int)c.y <= py && py < (int)(c.y + c.h)) return 1;
    }
    return 0;
}

/* the colour of the last span painted over (px,py), or 0 if none did */
static uint16_t rt_sample_at(int px, int py) {
    uint16_t v = 0;
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        mock_draw_call_t c = mock_driver_log[i];
        if ((int)c.x <= px && px < (int)(c.x + c.w) &&
            (int)c.y <= py && py < (int)(c.y + c.h)) v = c.sample_pixel;
    }
    return v;
}

/* glyph-blit counters — defined with the font fixtures further down */
static int count_calls_of_size(uint16_t w, uint16_t h);

/* does any span of colour `colour` touch the x-band [x_lo, x_hi)? */
static int rt_colour_in_xband(uint16_t colour, int x_lo, int x_hi) {
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        mock_draw_call_t c = mock_driver_log[i];
        if (c.sample_pixel != colour) continue;
        if ((int)c.x < x_hi && (int)(c.x + c.w) > x_lo) return 1;
    }
    return 0;
}

/* is some `colour` pixel within `tol` (Chebyshev) of `(x, y)`? Used by the
 * vu needle tests: ticks and the needle share `.color` and the same outer
 * radius (`len`), so a farthest-pixel scan can't tell them apart — instead
 * probe an *inner* radius (well under the ticks' 0.9*len..len band) that
 * only the needle's own hub-to-tip line ever reaches. */
static int rt_colour_near(uint16_t colour, int x, int y, int tol) {
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        mock_draw_call_t c = mock_driver_log[i];
        if (c.sample_pixel != colour) continue;
        int dx = (int)c.x - x;
        int dy = (int)c.y - y;
        if (dx < 0) dx = -dx;
        if (dy < 0) dy = -dy;
        if (dx <= tol && dy <= tol) return 1;
    }
    return 0;
}

/* same ray formula draw_vu uses internally (janus_runtime.c's vu_ray_x/y,
 * static there) — reimplemented here against the public janus_sin16/
 * janus_cos16 so the test can predict where the needle lands at a given
 * angle without reaching into runtime internals. */
static void rt_vu_ray_point(int hub_x, int hub_y, int16_t len, int16_t deg, int *out_x, int *out_y) {
    *out_x = hub_x + (int)(((int32_t)len * janus_sin16(deg)) >> 15);
    *out_y = hub_y - (int)(((int32_t)len * janus_cos16(deg)) >> 15);
}

/* blocking render == async-drained render, span for span — the
 * "drains only JANUS_ASYNC_OP_FILL" check: a non-fill op would replay
 * differently (or not at all) through the queue. */
static int rt_render_matches_async(const janus_screen_desc_t *screen) {
    static mock_draw_call_t blocking[MOCK_DRIVER_LOG_CAPACITY];
    mock_driver_reset();
    janus_render_screen(screen);
    uint16_t n = mock_driver_log_count;
    for (uint16_t i = 0; i < n; i++) blocking[i] = mock_driver_log[i];

    mock_driver_reset();
    janus_render_screen_async_start(screen);
    while (janus_render_poll()) { }
    if (mock_driver_log_count != n || n == 0) return 0;
    for (uint16_t i = 0; i < n; i++) {
        mock_draw_call_t a = mock_driver_log[i], b = blocking[i];
        if (a.x != b.x || a.y != b.y || a.w != b.w || a.h != b.h ||
            a.sample_pixel != b.sample_pixel) return 0;
    }
    return 1;
}

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

/* progress renders as a bar: recessed rounded track (.bg_color) + a
 * rounded proportional fill (.color) + a top gloss on the filled part.
 * Geometry {0,0,100,12}, radius 6, mid-height y=6 (below the gloss). */
#define PB_COLOR   0x1111
#define PB_BG      0xeeee

static const janus_widget_desc_t g_progress = {
    .kind = JANUS_WIDGET_PROGRESS, .id = "p", .geometry = { 0, 0, 100, 12 },
    .bind = {
        .field_offset = offsetof(demo_t, level), .field_type = JANUS_FIELD_INT,
        .range_min = 0, .range_max = 100,
    },
    .color = PB_COLOR, .bg_color = PB_BG,
};
static const janus_screen_desc_t g_progress_screen = {
    .name = "Progress", .widgets = &g_progress, .widget_count = 1, .bound_struct = &g_demo,
};

static void test_progress_bar_fill_width_tracks_value(void) {
    g_demo.level = 50;
    mock_driver_reset();
    janus_render_screen(&g_progress_screen);
    CHECK(rt_painted_colour(40, 6, PB_COLOR));     /* filled at ~40% width */
    CHECK(!rt_painted_colour(80, 6, PB_COLOR));    /* not at ~80% */
    CHECK(!rt_painted_colour(0, 0, PB_BG));         /* rounded track leaves its corner unfilled */

    g_demo.level = 0;
    mock_driver_reset();
    janus_render_screen(&g_progress_screen);
    CHECK(!rt_colour_in_xband(PB_COLOR, 0, 100));  /* nothing filled */
    CHECK(rt_painted_colour(50, 6, PB_BG));        /* bare track at the midline */

    g_demo.level = 100;
    mock_driver_reset();
    janus_render_screen(&g_progress_screen);
    CHECK(rt_painted_colour(98, 6, PB_COLOR));     /* filled right up to the end */
}

static void test_progress_render_is_async_safe(void) {
    g_demo.level = 60;
    CHECK(rt_render_matches_async(&g_progress_screen));
}

static void test_gauge_renders_identically_to_progress(void) {
    static const janus_widget_desc_t gauge = {
        .kind = JANUS_WIDGET_GAUGE, .id = "p", .geometry = { 0, 0, 100, 12 },
        .bind = {
            .field_offset = offsetof(demo_t, level), .field_type = JANUS_FIELD_INT,
            .range_min = 0, .range_max = 100,
        },
        .color = PB_COLOR, .bg_color = PB_BG,
    };
    static const janus_screen_desc_t gauge_screen = {
        .name = "Gauge", .widgets = &gauge, .widget_count = 1, .bound_struct = &g_demo,
    };
    static mock_draw_call_t prog[MOCK_DRIVER_LOG_CAPACITY];

    g_demo.level = 37;
    mock_driver_reset();
    janus_render_screen(&g_progress_screen);
    uint16_t n = mock_driver_log_count;
    for (uint16_t i = 0; i < n; i++) prog[i] = mock_driver_log[i];

    mock_driver_reset();
    janus_render_screen(&gauge_screen);
    CHECK(mock_driver_log_count == n && n > 0);
    for (uint16_t i = 0; i < mock_driver_log_count && i < n; i++) {
        CHECK(prog[i].x == mock_driver_log[i].x && prog[i].y == mock_driver_log[i].y);
        CHECK(prog[i].w == mock_driver_log[i].w && prog[i].h == mock_driver_log[i].h);
        CHECK(prog[i].sample_pixel == mock_driver_log[i].sample_pixel);
    }
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
 * consumed (cleared) on that redraw. On a *dirty sweep* (real
 * bound_dirty) an unbound (static) leaf is skipped — its pixels can't
 * change — while the force path (NULL bound_dirty) still draws it. ---- */
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

static void test_unbound_widget_skipped_on_a_dirty_sweep(void) {
    /* Same static label, but through a real bound_dirty struct: a repeated
     * dirty sweep must not repaint it (2026-09-07 -- stops a
     * timer-refreshed header flickering its unchanging text). */
    static dirty_demo_t data = { .level = 0 };
    static dirty_demo_dirty_t dirty = { .level = false };
    static const janus_widget_desc_t w = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = "hi", .geometry = { 0, 0, 20, 10 },
    };

    mock_driver_reset();
    janus_render_widget_if_dirty(&w, &data, &dirty);
    CHECK(mock_driver_log_count == 0);   /* unbound + dirty-aware sweep -> skipped */
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
    /* summary child is a checkbox (one flat fill) — this fixture asserts
     * exact call counts, and led now renders as a multi-span shaded disc
     * (kind_visuals task 3); the box-summary behaviour under test doesn't
     * depend on which leaf kind sits in the summary. */
    static const janus_widget_desc_t summary_dot = {
        .kind = JANUS_WIDGET_CHECKBOX, .id = "status_dot", .geometry = { 0, 0, 10, 10 },
    };
    static const janus_widget_desc_t box_child = {
        .kind = JANUS_WIDGET_LABEL, .id = "child", .geometry = { 0, 16, 10, 10 },
    };
    static const janus_widget_desc_t box_widget = {
        .kind = JANUS_WIDGET_BOX, .id = "box1",
        .geometry = { 0, 0, 10, 26 }, .geometry_collapsed = { 0, 0, 10, 16 },
        .initial_expanded = false,
        .children = &box_child, .child_count = 1,
        .summary_children = &summary_dot, .summary_child_count = 1,
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

/* ---- fixture 4: switching erases the outgoing screen, then draws only
 * the new one's widgets (never re-renders the old screen's content). ---- */
static void test_switch_screen_erases_old_then_draws_new(void) {
    static const janus_widget_desc_t s1_widget = {
        .kind = JANUS_WIDGET_LABEL, .id = "s1w", .geometry = { 0, 0, 10, 10 },
        .bg_color = 0x1234,
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
    CHECK(mock_driver_log_count == 2);          /* 1 union-bbox erase of s1 + 1 draw of s2's widget */
    CHECK(mock_driver_log[0].sample_pixel == 0x1234);  /* the erase uses s1's first widget's bg colour */
}

/* the erase is one fill over the union of the outgoing screen's top-level
 * rects, anchored at the origin — so it also covers the GAP between them
 * and any ragged right/bottom edge a per-widget fill would leave. */
static void test_switch_screen_erase_covers_gaps_and_ragged_edges(void) {
    static const janus_widget_desc_t s1_a = {
        .kind = JANUS_WIDGET_LABEL, .id = "a", .geometry = { 0, 0, 10, 10 }, .bg_color = 0x0777,
    };
    static const janus_widget_desc_t s1_b = {
        .kind = JANUS_WIDGET_LABEL, .id = "b", .geometry = { 0, 20, 40, 10 }, .bg_color = 0x0777,
    };
    static const janus_widget_desc_t s1_widgets[] = { s1_a, s1_b };
    static const janus_widget_desc_t s2_widget = {
        .kind = JANUS_WIDGET_LABEL, .id = "s2w", .geometry = { 0, 0, 4, 4 },
    };
    static const janus_screen_desc_t s1 = { .name = "S1", .widgets = s1_widgets, .widget_count = 2 };
    static const janus_screen_desc_t s2 = { .name = "S2", .widgets = &s2_widget, .widget_count = 1 };
    static const janus_screen_desc_t *const screens[] = { &s1, &s2 };
    janus_app_t app = {
        .screens = screens, .nav_titles = NULL, .screen_count = 2, .active_screen = 0,
    };

    mock_driver_reset();
    janus_switch_screen(&app, 1);
    CHECK(rt_painted_colour(5, 15, 0x0777));    /* the gap between the two rows */
    CHECK(rt_painted_colour(30, 5, 0x0777));    /* ragged edge past the narrow first row */
}

/* ---- fixture 4b: the app-level nav strip (nav_tabs epic task 2) ----
 * 3 tabs, 40px cells across a 120px band, NAV_BAR_H (28) tall. targets
 * are screen indices in nav order; the cell whose target == active_screen
 * is the "active" one. */
static const janus_nav_tab_t rt_nav_tabs[] = {
    { { 0,  0, 40, 28 }, "A", 0 },
    { { 40, 0, 40, 28 }, "B", 1 },
    { { 80, 0, 40, 28 }, "C", 2 },
};
static const janus_screen_desc_t rt_nav_s0 = { .name = "S0", .widget_count = 0 };
static const janus_screen_desc_t rt_nav_s1 = { .name = "S1", .widget_count = 0 };
static const janus_screen_desc_t rt_nav_s2 = { .name = "S2", .widget_count = 0 };
static const janus_screen_desc_t *const rt_nav_screens[] = { &rt_nav_s0, &rt_nav_s1, &rt_nav_s2 };

static void test_nav_bar_renders_cells_and_marks_the_active_tab(void) {
    janus_app_t app = {
        .screens = rt_nav_screens, .nav_titles = NULL,
        .nav_tabs = rt_nav_tabs, .nav_tab_count = 3,
        .screen_count = 3, .active_screen = 1,
    };

    mock_driver_reset();
    janus_render_nav_bar(&app);

    /* one fill per cell, spanning the whole 120px band, no gap */
    CHECK(rt_painted(2, 4) && rt_painted(60, 4) && rt_painted(118, 4));
    /* three medium-glyph title blits (one char each) */
    CHECK(count_calls_of_size(JANUS_FONT_MEDIUM_GLYPH_W, JANUS_FONT_MEDIUM_GLYPH_H) == 3);

    /* active cell (tab B, target 1) reads different from an inactive one */
    uint16_t active_body = rt_sample_at(60, 4);
    uint16_t inactive_body = rt_sample_at(20, 4);
    CHECK(active_body != inactive_body);

    /* accent bar: a ~6px band along the bottom of the active cell only */
    CHECK(rt_sample_at(60, 24) != active_body);            /* accent colour, not the cell body */
    CHECK(rt_sample_at(60, 24) == rt_sample_at(60, 27));   /* same accent colour top-to-bottom of the bar */
    CHECK(rt_sample_at(20, 24) == inactive_body);          /* inactive cell: no accent band this high */
}

static void test_nav_bar_no_op_without_nav(void) {
    janus_app_t no_nav = {
        .screens = rt_nav_screens, .nav_titles = NULL,
        .nav_tabs = NULL, .nav_tab_count = 0,
        .screen_count = 3, .active_screen = 0,
    };
    mock_driver_reset();
    janus_render_nav_bar(&no_nav);
    janus_render_nav_bar(NULL);
    CHECK(mock_driver_log_count == 0);
}

static void test_switch_screen_repaints_the_nav_strip(void) {
    janus_app_t app = {
        .screens = rt_nav_screens, .nav_titles = NULL,
        .nav_tabs = rt_nav_tabs, .nav_tab_count = 3,
        .screen_count = 3, .active_screen = 0,
    };
    /* active tab A -> its cell [0,40) carries the accent at y=24 */
    janus_set_focus(NULL);   /* drop any stale focus a prior fixture left set */
    mock_driver_reset();
    janus_render_nav_bar(&app);
    uint16_t a_accent = rt_sample_at(20, 24);
    uint16_t a_body = rt_sample_at(20, 4);
    CHECK(a_accent != a_body);

    mock_driver_reset();
    janus_switch_screen(&app, 2);          /* -> active tab C, cell [80,120) */
    CHECK(app.active_screen == 2);
    CHECK(rt_sample_at(100, 24) == a_accent);   /* accent moved to C's cell */
    CHECK(rt_sample_at(20, 24) != a_accent);    /* and left A's cell */
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

/* toggle now renders a switch: a rounded track (its colour = the showing
 * state) + a circular knob that sits left when off, right when on. Knob
 * colour is a lightened copy of the track. Geometry {0,0,60,16}: knob
 * diameter 16, radius 7; off-knob centred at x=9 (covers ~[2,16]),
 * on-knob at x=51 (covers ~[44,58]); x=20..40 mid-height is track-only. */
static void test_toggle_renders_a_switch_tracking_state(void) {
    static const janus_widget_desc_t toggle = {
        .kind = JANUS_WIDGET_TOGGLE, .id = "t", .geometry = { 0, 0, 60, 16 },
        .bind = { .field_offset = offsetof(demo_t, level), .field_type = JANUS_FIELD_INT },
        .color = 0x001F, .bg_color = 0xF800,
    };
    static const janus_screen_desc_t screen = {
        .name = "Toggle", .widgets = &toggle, .widget_count = 1, .bound_struct = &g_demo,
    };
    const uint16_t knob_off = janus_rgb565_lerp(0xF800, 0xFFFF, 96);
    const uint16_t knob_on  = janus_rgb565_lerp(0x001F, 0xFFFF, 96);

    g_demo.level = 0;
    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(rt_colour_in_xband(knob_off, 0, 20));    /* knob on the left */
    CHECK(!rt_colour_in_xband(knob_off, 40, 60));  /* ...and not on the right */
    CHECK(rt_painted_colour(20, 8, 0xF800));       /* track shows the OFF colour */
    CHECK(!rt_painted(0, 0));                       /* pill track: corner is clipped off */

    g_demo.level = 1;
    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(rt_colour_in_xband(knob_on, 40, 60));    /* knob on the right */
    CHECK(!rt_colour_in_xband(knob_on, 0, 20));    /* ...and not on the left */
    CHECK(rt_painted_colour(20, 8, 0x001F));       /* track shows the ON colour */
}

static void test_toggle_render_is_async_safe(void) {
    static const janus_widget_desc_t toggle = {
        .kind = JANUS_WIDGET_TOGGLE, .id = "t", .geometry = { 0, 0, 60, 16 },
        .bind = { .field_offset = offsetof(demo_t, level), .field_type = JANUS_FIELD_INT },
        .color = 0x001F, .bg_color = 0xF800,
    };
    static const janus_screen_desc_t screen = {
        .name = "ToggleAsync", .widgets = &toggle, .widget_count = 1, .bound_struct = &g_demo,
    };
    g_demo.level = 1;
    CHECK(rt_render_matches_async(&screen));
}

/* led renders as a shaded disc: darker rim ring, state-colour face,
 * lighter specular highlight up-left. Geometry {0,0,20,20}: cx=cy=10,
 * rad=10, highlight centred at (7,7). */
#define LED_COLOR  0x4208   /* mid grey with headroom both ways */
#define LED_BG     0x8410

static const janus_widget_desc_t g_led = {
    .kind = JANUS_WIDGET_LED, .id = "led", .geometry = { 0, 0, 20, 20 },
    .bind = { .field_offset = offsetof(demo_t, level), .field_type = JANUS_FIELD_INT },
    .color = LED_COLOR, .bg_color = LED_BG,
};
static const janus_screen_desc_t g_led_screen = {
    .name = "Led", .widgets = &g_led, .widget_count = 1, .bound_struct = &g_demo,
};

static int channel_lighter(uint16_t lighter, uint16_t base) {
    return ((lighter >> 11) & 0x1F) >= ((base >> 11) & 0x1F) &&
           ((lighter >> 5)  & 0x3F) >= ((base >> 5)  & 0x3F) &&
           ( lighter        & 0x1F) >= ( base        & 0x1F) &&
           lighter != base;
}

static void test_led_shaded_disc_per_state(void) {
    const uint16_t rim = janus_rgb565_lerp(LED_COLOR, 0x0000, 80);
    const uint16_t hl  = janus_rgb565_lerp(LED_COLOR, 0xFFFF, 130);

    g_demo.level = 1;
    mock_driver_reset();
    janus_render_screen(&g_led_screen);
    CHECK(rt_painted_colour(10, 10, LED_COLOR));    /* face centre = state colour */
    CHECK(rt_painted_colour(20, 10, rim));          /* rim ring at the disc edge */
    CHECK(rim != LED_COLOR && rim != 0);
    CHECK(rt_painted_colour(7, 7, hl));             /* specular highlight */
    CHECK(channel_lighter(hl, LED_COLOR));
    CHECK(!rt_painted(22, 10));                     /* nothing outside the disc */

    g_demo.level = 0;
    mock_driver_reset();
    janus_render_screen(&g_led_screen);
    CHECK(rt_painted_colour(10, 10, LED_BG));       /* off state */

    g_demo.level = 2;
    mock_driver_reset();
    janus_render_screen(&g_led_screen);
    CHECK(rt_painted_colour(10, 10, JANUS_COLOR_LED_WARN));  /* warn state */
}

static void test_led_render_is_async_safe(void) {
    g_demo.level = 1;
    CHECK(rt_render_matches_async(&g_led_screen));
}

/* vu: analog needle over a 90 degree tick arc (vu_meter task 2). Geometry
 * {0,0,80,48}: hub at (40,47) (bottom-centre), len = min(40,48) - 2 = 38.
 * value==min -> needle at -45 deg (up-left), value==max -> +45 deg
 * (up-right), midpoint -> 0 deg (straight up). Ticks share `.color` with
 * the needle and sit at the same outer radius, so "the farthest `.color`
 * pixel from the hub" may land on a tick instead of the needle tip at an
 * extreme value — harmless here, since the nearest tick at an extreme
 * points the same general direction the needle does. */
#define VU_COLOR 0x001F
#define VU_BG    0xF800
#define VU_HUB_X 40
#define VU_HUB_Y 47
#define VU_LEN   38   /* min(r.w/2, r.h) - 2 = min(40, 48) - 2 */
#define VU_INNER_R 15 /* well under the ticks' 0.9*VU_LEN..VU_LEN band (~34..38) */

static const janus_widget_desc_t g_vu = {
    .kind = JANUS_WIDGET_VU, .id = "v", .geometry = { 0, 0, 80, 48 },
    .bind = {
        .field_offset = offsetof(demo_t, level), .field_type = JANUS_FIELD_INT,
        .range_min = 0, .range_max = 100,
    },
    .color = VU_COLOR, .bg_color = VU_BG,
};
static const janus_screen_desc_t g_vu_screen = {
    .name = "Vu", .widgets = &g_vu, .widget_count = 1, .bound_struct = &g_demo,
};

static void test_vu_face_fill_under_the_needle(void) {
    g_demo.level = 50;
    mock_driver_reset();
    janus_render_screen(&g_vu_screen);
    CHECK(rt_painted_colour(0, 0, VU_BG));    /* far corner: face fill, untouched by needle/ticks/hub */
    CHECK(!rt_painted(85, 20));               /* nothing drawn outside the geometry */
}

static void test_vu_needle_points_up_left_at_range_min(void) {
    int ex, ey;
    rt_vu_ray_point(VU_HUB_X, VU_HUB_Y, VU_INNER_R, -45, &ex, &ey);
    CHECK(ex < VU_HUB_X);
    CHECK(ey < VU_HUB_Y);

    g_demo.level = 0;
    mock_driver_reset();
    janus_render_screen(&g_vu_screen);
    CHECK(rt_colour_near(VU_COLOR, ex, ey, 1));
}

static void test_vu_needle_points_up_right_at_range_max(void) {
    int ex, ey;
    rt_vu_ray_point(VU_HUB_X, VU_HUB_Y, VU_INNER_R, 45, &ex, &ey);
    CHECK(ex > VU_HUB_X);
    CHECK(ey < VU_HUB_Y);

    g_demo.level = 100;
    mock_driver_reset();
    janus_render_screen(&g_vu_screen);
    CHECK(rt_colour_near(VU_COLOR, ex, ey, 1));
}

static void test_vu_needle_points_straight_up_at_midpoint(void) {
    int ex, ey;
    rt_vu_ray_point(VU_HUB_X, VU_HUB_Y, VU_INNER_R, 0, &ex, &ey);
    CHECK(ex - VU_HUB_X <= 2 && VU_HUB_X - ex <= 2);
    CHECK(ey < VU_HUB_Y);

    g_demo.level = 50;
    mock_driver_reset();
    janus_render_screen(&g_vu_screen);
    CHECK(rt_colour_near(VU_COLOR, ex, ey, 1));
}

static void test_vu_clamps_above_range_max(void) {
    int ex, ey;
    rt_vu_ray_point(VU_HUB_X, VU_HUB_Y, VU_INNER_R, 45, &ex, &ey);

    g_demo.level = 150;   /* above range_max=100 — must render exactly like level == 100 */
    mock_driver_reset();
    janus_render_screen(&g_vu_screen);
    CHECK(rt_colour_near(VU_COLOR, ex, ey, 1));
}

static void test_vu_hub_painted_for_every_value(void) {
    int values[] = { 0, 25, 50, 75, 100 };
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) {
        g_demo.level = values[i];
        mock_driver_reset();
        janus_render_screen(&g_vu_screen);
        CHECK(rt_painted_colour(VU_HUB_X, VU_HUB_Y, VU_COLOR));
    }
}

static void test_vu_render_is_async_safe(void) {
    g_demo.level = 50;
    CHECK(rt_render_matches_async(&g_vu_screen));
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

static void test_text_wider_than_widget_shrinks_to_fit(void) {
    /* "AB" at large (1px pad + 2*20 + 1 gap = 42px) doesn't fit w=24, so
     * draw_string steps the font down: large -> medium makes it
     * 1 + 2*10 + 1 = 22px, which fits. Both glyphs draw, at the medium
     * size — nothing at large, nothing clipped (2026-09-07: render-time
     * auto-shrink). */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = "AB", .geometry = { 0, 0, 24, 16 },
    };
    static const janus_screen_desc_t screen = {
        .name = "Shrunk", .widgets = &label, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(count_glyph_sized_calls() == 0);                                             /* nothing at large */
    CHECK(count_calls_of_size(JANUS_FONT_MEDIUM_GLYPH_W, JANUS_FONT_MEDIUM_GLYPH_H) == 2);
}

static void test_text_still_clips_once_at_the_smallest_size(void) {
    /* "ABCD" can't fit w=21 even at medium (4*(10+1)-1 = 43px) and there's
     * no smaller table, so the old clip behaviour still applies at the
     * floor: only the glyphs that fit are drawn ('A' at x=1, 'B' would
     * start at x=12 and end at 22 > 21 -> dropped). */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l", .static_text = "ABCD", .geometry = { 0, 0, 21, 16 },
        .font_size = JANUS_FONT_SIZE_MEDIUM,
    };
    static const janus_screen_desc_t screen = {
        .name = "ClippedFloor", .widgets = &label, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(count_calls_of_size(JANUS_FONT_MEDIUM_GLYPH_W, JANUS_FONT_MEDIUM_GLYPH_H) == 1);
}

static void test_button_text_overflow_clips_not_shrinks(void) {
    /* A button does NOT auto-shrink (unlike label/header) — a tab bar must
     * keep one consistent size, so an over-long button label clips at the
     * authored font instead of silently dropping to medium (2026-09-07
     * round 2). "ABCD" at large needs 1 + 4*20 + 3 = 84px; in w=45 only
     * 'A' (x=1..21) and 'B' (x=22..42) fit, 'C' would end at 63 > 45. */
    static const janus_widget_desc_t button = {
        .kind = JANUS_WIDGET_BUTTON, .id = "b", .static_text = "ABCD", .geometry = { 0, 0, 45, 28 },
    };
    static const janus_screen_desc_t screen = {
        .name = "ClipBtn", .widgets = &button, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(count_glyph_sized_calls() == 2);   /* still at large, just clipped */
    CHECK(count_calls_of_size(JANUS_FONT_MEDIUM_GLYPH_W, JANUS_FONT_MEDIUM_GLYPH_H) == 0);
}

static void test_button_text_is_centered(void) {
    /* A short label in a wide button sits centered, not jammed against the
     * left edge (2026-09-07). "A" is one 20px glyph in a 60px button:
     * left-aligned x would be 1; centered x is 0 + (60 - 20)/2 = 20. */
    static const janus_widget_desc_t button = {
        .kind = JANUS_WIDGET_BUTTON, .id = "b", .static_text = "A", .geometry = { 0, 0, 60, 28 },
    };
    static const janus_screen_desc_t screen = {
        .name = "CenteredBtn", .widgets = &button, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_render_screen(&screen);
    int found = 0;
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        if (mock_driver_log[i].w == JANUS_FONT_LARGE_GLYPH_W &&
            mock_driver_log[i].h == JANUS_FONT_LARGE_GLYPH_H) {
            found = 1;
            CHECK(mock_driver_log[i].x == 20);   /* centered, not 1 */
        }
    }
    CHECK(found);
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

/* ---- fixture 6b: label_format — printf template in `text:` ---- */

typedef struct { int32_t n; float f; } demo_num_t;
static demo_num_t g_demo_num = { 0, 0.0f };

static void test_format_label_interpolates_int_value(void) {
    /* "%d%%" over an int bind of 72 -> "72%", three glyphs. w=64 fits
     * three 20px columns (1 + 20 + 1 + 20 + 1 + 20 = 63). */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l",
        .static_text = "%d%%", .text_is_format = true,
        .geometry = { 0, 0, 64, 10 },
        .bind = { .field_offset = offsetof(demo_num_t, n), .field_type = JANUS_FIELD_INT },
    };
    static const janus_screen_desc_t screen = {
        .name = "FmtInt", .widgets = &label, .widget_count = 1, .bound_struct = &g_demo_num,
    };

    g_demo_num.n = 72;
    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(count_glyph_sized_calls() == 3);  /* '7' '2' '%' */
}

static void test_format_label_interpolates_float_value(void) {
    /* "%.1f" over a float bind of 21.5 -> "21.5", four glyphs. w=96 fits
     * four 20px columns comfortably. */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l",
        .static_text = "%.1f", .text_is_format = true,
        .geometry = { 0, 0, 96, 10 },
        .bind = { .field_offset = offsetof(demo_num_t, f), .field_type = JANUS_FIELD_FLOAT },
    };
    static const janus_screen_desc_t screen = {
        .name = "FmtFloat", .widgets = &label, .widget_count = 1, .bound_struct = &g_demo_num,
    };

    g_demo_num.f = 21.5f;
    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(count_glyph_sized_calls() == 4);  /* '2' '1' '.' '5' */
}

static void test_format_false_label_still_blits_static_text_verbatim(void) {
    /* text_is_format = false: a literal string with no conversion is
     * drawn straight from flash, bound value ignored (tie-break). */
    static const janus_widget_desc_t label = {
        .kind = JANUS_WIDGET_LABEL, .id = "l",
        .static_text = "AB", .text_is_format = false,
        .geometry = { 0, 0, 64, 10 },
        .bind = { .field_offset = offsetof(demo_num_t, n), .field_type = JANUS_FIELD_INT },
    };
    static const janus_screen_desc_t screen = {
        .name = "FmtOff", .widgets = &label, .widget_count = 1, .bound_struct = &g_demo_num,
    };

    g_demo_num.n = 999;
    mock_driver_reset();
    janus_render_screen(&screen);
    CHECK(count_glyph_sized_calls() == 2);  /* "AB", not "999" */
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
    test_progress_bar_fill_width_tracks_value();
    test_progress_render_is_async_safe();
    test_gauge_renders_identically_to_progress();
    test_render_widget_draws_only_that_widget();
    test_render_widget_if_dirty_skips_unchanged_field();
    test_unbound_widget_always_draws_via_if_dirty();
    test_unbound_widget_skipped_on_a_dirty_sweep();
    test_render_screen_if_dirty_respects_the_bit();
    test_box_collapse_and_toggle();
    test_toggle_box_clears_vacated_body_on_collapse();
    test_box_summary_renders_when_collapsed_and_expanded();
    test_switch_screen_erases_old_then_draws_new();
    test_switch_screen_erase_covers_gaps_and_ragged_edges();
    test_nav_bar_renders_cells_and_marks_the_active_tab();
    test_nav_bar_no_op_without_nav();
    test_switch_screen_repaints_the_nav_strip();
    test_divider_always_draws_unconditionally();
    test_toggle_renders_a_switch_tracking_state();
    test_toggle_render_is_async_safe();
    test_led_shaded_disc_per_state();
    test_led_render_is_async_safe();
    test_vu_face_fill_under_the_needle();
    test_vu_needle_points_up_left_at_range_min();
    test_vu_needle_points_up_right_at_range_max();
    test_vu_needle_points_straight_up_at_midpoint();
    test_vu_clamps_above_range_max();
    test_vu_hub_painted_for_every_value();
    test_vu_render_is_async_safe();
    test_badge_fill_tracks_live_value();
    test_slider_fill_tracks_live_value();
    test_label_without_text_draws_only_the_background_fill();
    test_label_with_text_draws_one_glyph_call_per_character();
    test_text_wider_than_widget_shrinks_to_fit();
    test_text_still_clips_once_at_the_smallest_size();
    test_button_text_overflow_clips_not_shrinks();
    test_button_text_is_centered();
    test_medium_font_size_draws_medium_sized_glyph();
    test_font_scale_multiplies_medium_up_to_large_footprint();
    test_font_scale_beyond_the_cap_is_clamped_not_overflowed();
    test_bound_string_with_value_draws_glyphs();
    test_bound_string_null_renders_fill_only();
    test_static_text_wins_over_bound_string();
    test_format_label_interpolates_int_value();
    test_format_label_interpolates_float_value();
    test_format_false_label_still_blits_static_text_verbatim();
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
