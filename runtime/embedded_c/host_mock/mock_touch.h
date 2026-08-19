/* Host-side touch source for testing/dev — implements the
 * janus_touch_poll() contract (janus_input_touch.h) by draining a small
 * queue instead of reading real hardware. Dev/test tooling only, not
 * part of the shipped fixed runtime library.
 */
#ifndef JANUS_HOST_MOCK_TOUCH_H
#define JANUS_HOST_MOCK_TOUCH_H

#include <stdint.h>

/* Queues one simulated tap at (x, y); janus_touch_poll() drains these
 * in order, one per call. Silently drops taps past the queue's small
 * fixed capacity — a test/demo bug, not something to grow unboundedly. */
void mock_touch_queue_tap(int16_t x, int16_t y);
void mock_touch_reset(void);

#endif /* JANUS_HOST_MOCK_TOUCH_H */
