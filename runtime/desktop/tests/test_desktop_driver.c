/* Tests for the SDL2 desktop driver itself (janus_desktop_driver.c) —
 * not janus_runtime.c's logic, which the mock-based tests already cover.
 * Runs headless: CMake sets SDL_VIDEODRIVER=dummy for this test only.
 * Plain assert-based, same style as test_runtime.c. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <SDL.h>

#include "janus_desktop_driver.h"
#include "janus_runtime.h"

#define PANEL_W 32
#define PANEL_H 24

#define BG 0x1111
#define FG 0xF81F

static uint16_t g_shot[PANEL_W * PANEL_H];

/* The driver keeps its renderer private; the first (only) window's
 * renderer is reachable through SDL itself. */
static void screenshot(void) {
    SDL_Window *win = SDL_GetWindowFromID(1);
    assert(win != NULL);
    SDL_Renderer *r = SDL_GetRenderer(win);
    assert(r != NULL);
    int rc = SDL_RenderReadPixels(r, NULL, SDL_PIXELFORMAT_RGB565, g_shot,
                                  PANEL_W * (int)sizeof(uint16_t));
    assert(rc == 0);
    (void)rc;
}

static void fill_panel(uint16_t color) {
    static uint16_t full[PANEL_W * PANEL_H];
    for (int i = 0; i < PANEL_W * PANEL_H; i++) full[i] = color;
    draw_area_sync(0, 0, PANEL_W, PANEL_H, full);
}

/* Asserts g_shot is BG everywhere except the (x,y,w,h) rect, which is FG. */
static void assert_rect_only(int rx, int ry, int rw, int rh) {
    for (int y = 0; y < PANEL_H; y++) {
        for (int x = 0; x < PANEL_W; x++) {
            bool inside = x >= rx && x < rx + rw && y >= ry && y < ry + rh;
            uint16_t want = inside ? FG : BG;
            if (g_shot[y * PANEL_W + x] != want) {
                fprintf(stderr, "pixel (%d,%d): got 0x%04X want 0x%04X\n",
                        x, y, g_shot[y * PANEL_W + x], want);
                assert(0);
            }
        }
    }
}

static void test_sync_paints_region_only(void) {
    fill_panel(BG);
    uint16_t buf[5 * 4];
    for (int i = 0; i < 5 * 4; i++) buf[i] = FG;
    draw_area_sync(7, 9, 5, 4, buf);
    screenshot();
    assert_rect_only(7, 9, 5, 4);
}

static void test_async_matches_sync(void) {
    fill_panel(BG);
    uint16_t buf[5 * 4];
    for (int i = 0; i < 5 * 4; i++) buf[i] = FG;
    bool accepted = draw_area_async(7, 9, 5, 4, buf);
    assert(accepted);
    (void)accepted;
    screenshot();
    assert_rect_only(7, 9, 5, 4);
}

static void test_display_never_busy(void) {
    assert(!display_busy());
    fill_panel(BG);
    assert(!display_busy());
}

static void test_pump_and_quit(void) {
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    assert(janus_desktop_driver_pump());

    SDL_Event quit;
    memset(&quit, 0, sizeof quit);
    quit.type = SDL_QUIT;
    assert(SDL_PushEvent(&quit) == 1);
    assert(!janus_desktop_driver_pump());
}

int main(void) {
    assert(janus_desktop_driver_init(PANEL_W, PANEL_H, "janus driver test"));

    test_sync_paints_region_only();
    test_async_matches_sync();
    test_display_never_busy();
    test_pump_and_quit();

    janus_desktop_driver_shutdown();
    puts("janus_desktop_driver_tests: OK");
    return 0;
}
