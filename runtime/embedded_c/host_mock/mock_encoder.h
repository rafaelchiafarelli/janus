/* Host-side encoder source for testing/dev — implements the
 * janus_encoder_poll() contract (janus_input_encoder.h) by draining a
 * small queue instead of reading real hardware. Dev/test tooling only,
 * not part of the shipped fixed runtime library.
 */
#ifndef JANUS_HOST_MOCK_ENCODER_H
#define JANUS_HOST_MOCK_ENCODER_H

#include <stdint.h>

/* Queues one simulated rotation/click; janus_encoder_poll() drains these
 * in order, one per call. Silently drops events past the queue's small
 * fixed capacity — a test/demo bug, not something to grow unboundedly. */
void mock_encoder_queue_rotate(int16_t delta);
void mock_encoder_queue_click(void);
void mock_encoder_reset(void);

#endif /* JANUS_HOST_MOCK_ENCODER_H */
