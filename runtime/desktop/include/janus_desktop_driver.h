/* Desktop (SDL2) display driver — the desktop target's implementation
 * of the fixed runtime's driver contract (draw_area_sync/draw_area_async
 * /display_busy, declared in janus_runtime.h, vendored unchanged from
 * runtime/embedded_c). Display only: no touch/encoder/button input is
 * interpreted here — janus_desktop_driver_pump() drains the SDL2 event
 * queue only enough to keep the window responsive and detect a close
 * request. Mapping real keyboard/mouse input to Janus's touch/encoder/
 * button poll contracts is the scaffold's (and ultimately the
 * consumer's own) job, same as real hardware signals are never
 * Janus's to interpret either.
 */
#ifndef JANUS_DESKTOP_DRIVER_H
#define JANUS_DESKTOP_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

/* Creates an SDL2 window + renderer sized (w, h) RGB565 pixels and a
 * matching streaming texture as the panel's backing store. Call once
 * at startup, before any janus_render_* call. Returns false on any
 * SDL2 failure (window/renderer/texture creation). */
bool janus_desktop_driver_init(uint16_t w, uint16_t h, const char *title);

/* Drains the SDL2 event queue. Returns false once the window's close
 * button (SDL_QUIT) was requested — the scaffold's main loop exits on
 * false — true otherwise. Interprets no other event. */
bool janus_desktop_driver_pump(void);

/* Destroys the texture/renderer/window. */
void janus_desktop_driver_shutdown(void);

#endif /* JANUS_DESKTOP_DRIVER_H */
