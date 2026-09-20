/* Desktop input for an encoder-modality app: the keyboard stands in for
 * the rotary encoder. Scaffolded once by Janus, then yours — edit freely.
 *
 *   Left / Right arrow  -> rotate -1 / +1 (one detent per press)
 *   Enter               -> the encoder's push-button
 *
 * Polls SDL *state* (not events): janus_desktop_driver_pump() drains the
 * event queue to detect the close button, so events never reach here.
 * Holding a key does not repeat — state polling only sees press edges. */
#include <stdbool.h>
#include <stdint.h>

#include <SDL.h>

#include "janus_input_encoder.h"

typedef struct {
    SDL_Scancode key;
    janus_encoder_event_t event;
    int16_t delta;
    bool was_down;
} binding_t;

static binding_t g_bindings[] = {
    { SDL_SCANCODE_LEFT,   JANUS_ENCODER_ROTATE, -1, false },
    { SDL_SCANCODE_RIGHT,  JANUS_ENCODER_ROTATE, +1, false },
    { SDL_SCANCODE_RETURN, JANUS_ENCODER_CLICK,   0, false },
};

/* At most one event per call. A key's edge is only consumed when it is
 * returned, so two keys pressed in the same frame both get delivered
 * (on consecutive calls) rather than the second being lost. */
bool janus_encoder_poll(janus_encoder_event_t *event, int16_t *delta) {
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    for (unsigned i = 0; i < sizeof g_bindings / sizeof g_bindings[0]; i++) {
        bool down = keys[g_bindings[i].key] != 0;
        if (down && !g_bindings[i].was_down) {
            g_bindings[i].was_down = true;
            *event = g_bindings[i].event;
            *delta = g_bindings[i].delta;
            return true;
        }
        g_bindings[i].was_down = down;
    }
    return false;
}
