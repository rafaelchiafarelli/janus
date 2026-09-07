/* janus_clear_screen's full-panel path — only compiled in when a project
 * declares `display.background` (app.yaml), which makes scaffold mode emit
 * JANUS_DISPLAY_BACKGROUND + JANUS_DISPLAY_PANEL_W/H into
 * janus_render_config.gen.h. This target fakes that by defining them on
 * the command line (see CMakeLists.txt). The default runtime suite
 * (test_runtime.c) covers the best-effort fallback with those macros
 * absent.
 */
#include <stdint.h>
#include <stdio.h>

#include "janus_runtime.h"
#include "mock_driver.h"

static int g_failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { g_failures++; fprintf(stderr, "FAIL: %s (%s:%d)\n", #cond, __FILE__, __LINE__); } \
} while (0)

/* was (px,py) painted `colour` by some logged span? */
static int painted_colour(int px, int py, uint16_t colour) {
    for (uint16_t i = 0; i < mock_driver_log_count; i++) {
        mock_draw_call_t c = mock_driver_log[i];
        if (c.sample_pixel != colour) continue;
        if ((int)c.x <= px && px < (int)(c.x + c.w) &&
            (int)c.y <= py && py < (int)(c.y + c.h)) return 1;
    }
    return 0;
}

/* A screen whose widgets cover only a small corner — the fallback would
 * leave most of the panel untouched; the full-panel path must not. */
static void test_declared_background_clears_the_whole_panel(void) {
    static const janus_widget_desc_t w = {
        .kind = JANUS_WIDGET_LABEL, .id = "w", .geometry = { 0, 0, 8, 8 }, .bg_color = 0x0001,
    };
    static const janus_screen_desc_t screen = {
        .name = "Bg", .widgets = &w, .widget_count = 1, .bound_struct = NULL,
    };

    mock_driver_reset();
    janus_clear_screen(&screen);

    /* JANUS_DISPLAY_PANEL_W/H = 48/32, JANUS_DISPLAY_BACKGROUND = 0x4321 */
    CHECK(painted_colour(0, 0, 0x4321));
    CHECK(painted_colour(40, 28, 0x4321));   /* far corner, well outside w's 8x8 rect */
    CHECK(painted_colour(24, 16, 0x4321));   /* centre */
    CHECK(!painted_colour(24, 16, 0x0001));  /* the widget's own bg was NOT used */
}

int main(void) {
    test_declared_background_clears_the_whole_panel();
    if (g_failures == 0) { printf("all clear_screen_bg tests passed\n"); return 0; }
    fprintf(stderr, "%d check(s) failed\n", g_failures);
    return 1;
}
