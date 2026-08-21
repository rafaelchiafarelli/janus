/* Janus embedded-C runtime — Stage 6, encoder modality. See architecture.md.
 *
 * Header-only, same as draw_area_sync/display_busy in janus_runtime.h and
 * janus_touch_poll in janus_input_touch.h — a driver contract, not
 * fixed-library logic, so there's no janus_input_encoder.c. Moving focus
 * and activating whatever's focused is identical to the button modality
 * and lives once in janus_input_focus.h; this header's only job is the
 * rotate/click event shape a rotary-encoder driver reports.
 */
#ifndef JANUS_INPUT_ENCODER_H
#define JANUS_INPUT_ENCODER_H

#include "janus_runtime.h"

typedef enum {
    JANUS_ENCODER_ROTATE,   /* `delta` is meaningful: +1 per detent clockwise, -1 counter-clockwise */
    JANUS_ENCODER_CLICK,    /* the encoder's integrated push-button; `delta` is unused */
} janus_encoder_event_t;

/* Input driver contract — vendor/host-provided, non-blocking, same
 * polling style as janus_touch_poll. Returns true and fills `event` (and
 * `delta` for JANUS_ENCODER_ROTATE) exactly once per new rotation/click;
 * false if there's nothing new since the last call. Coalescing multiple
 * detents into one ROTATE call (a larger `delta`) is a driver's choice,
 * not this contract's. */
bool janus_encoder_poll(janus_encoder_event_t *event, int16_t *delta);

#endif /* JANUS_INPUT_ENCODER_H */
