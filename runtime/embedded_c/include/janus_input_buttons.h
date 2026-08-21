/* Janus embedded-C runtime — Stage 6, discrete push-button modality.
 * See architecture.md.
 *
 * Header-only, same as janus_input_encoder.h — a driver contract, not
 * fixed-library logic, so there's no janus_input_buttons.c. NEXT/PREV
 * map directly onto janus_focus_move, SELECT onto janus_focus_activate
 * (janus_input_focus.h) — the same shared core the encoder modality
 * uses, just driven by discrete GPIO edges instead of a rotation.
 */
#ifndef JANUS_INPUT_BUTTONS_H
#define JANUS_INPUT_BUTTONS_H

#include "janus_runtime.h"

typedef enum {
    JANUS_BUTTON_NEXT,      /* move focus forward one — janus_focus_move(screen, 1) */
    JANUS_BUTTON_PREV,      /* move focus back one — janus_focus_move(screen, -1) */
    JANUS_BUTTON_SELECT,    /* activate whatever's focused — janus_focus_activate(screen) */
} janus_button_event_t;

/* Input driver contract — vendor/host-provided, non-blocking, same
 * polling style as janus_touch_poll/janus_encoder_poll. Returns true and
 * fills `event` exactly once per new button edge; false if there's
 * nothing new since the last call. */
bool janus_buttons_poll(janus_button_event_t *event);

#endif /* JANUS_INPUT_BUTTONS_H */
