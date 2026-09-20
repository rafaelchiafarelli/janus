#include "janus_desktop_driver.h"
#include "janus_runtime.h" /* declares the draw_area_sync/async/display_busy
                             * contract this file implements */

#include <SDL.h>

static SDL_Window *g_window = NULL;
static SDL_Renderer *g_renderer = NULL;
static SDL_Texture *g_texture = NULL;

bool janus_desktop_driver_init(uint16_t w, uint16_t h, const char *title) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;

    g_window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                 (int)w, (int)h, SDL_WINDOW_SHOWN);
    if (g_window == NULL) return false;

    /* The dummy video driver (headless tests) has no GPU — fall back to
     * the software renderer when hardware acceleration isn't available. */
    g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED);
    if (g_renderer == NULL) {
        g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (g_renderer == NULL) return false;

    /* One streaming texture is the whole panel's backing store — the
     * fixed runtime already treats the display as one addressable
     * RGB565 surface, so no per-widget texture bookkeeping is needed. */
    g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB565,
                                   SDL_TEXTUREACCESS_STREAMING, (int)w, (int)h);
    if (g_texture == NULL) return false;

    return true;
}

static void blit(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels) {
    SDL_Rect rect;
    rect.x = (int)x;
    rect.y = (int)y;
    rect.w = (int)w;
    rect.h = (int)h;
    /* pixels is a tightly-packed w*h RGB565 buffer for just this
     * sub-rect (the driver contract's own shape) — pitch is w pixels. */
    SDL_UpdateTexture(g_texture, &rect, pixels, (int)(w * sizeof(uint16_t)));
    SDL_RenderCopy(g_renderer, g_texture, NULL, NULL);
    SDL_RenderPresent(g_renderer);
}

void draw_area_sync(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels) {
    blit(x, y, w, h, pixels);
}

bool draw_area_async(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint16_t *pixels) {
    /* Always instant — SDL2's present is fast enough that the tiled,
     * backoff-on-busy path this exists for on real SPI panels has no
     * reason to exist here (desktop_sdl2_runtime epic decision 5). */
    blit(x, y, w, h, pixels);
    return true;
}

bool display_busy(void) {
    return false;
}

bool janus_desktop_driver_pump(void) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) return false;
    }
    return true;
}

void janus_desktop_driver_shutdown(void) {
    if (g_texture != NULL) { SDL_DestroyTexture(g_texture); g_texture = NULL; }
    if (g_renderer != NULL) { SDL_DestroyRenderer(g_renderer); g_renderer = NULL; }
    if (g_window != NULL) { SDL_DestroyWindow(g_window); g_window = NULL; }
    SDL_Quit();
}
