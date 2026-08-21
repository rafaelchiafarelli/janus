/* Host-side push-button source for testing/dev — implements the
 * janus_buttons_poll() contract (janus_input_buttons.h) by draining a
 * small queue instead of reading real hardware. Dev/test tooling only,
 * not part of the shipped fixed runtime library.
 */
#ifndef JANUS_HOST_MOCK_BUTTONS_H
#define JANUS_HOST_MOCK_BUTTONS_H

#include "janus_input_buttons.h"

/* Queues one simulated button edge; janus_buttons_poll() drains these in
 * order, one per call. Silently drops events past the queue's small
 * fixed capacity — a test/demo bug, not something to grow unboundedly. */
void mock_buttons_queue(janus_button_event_t event);
void mock_buttons_reset(void);

#endif /* JANUS_HOST_MOCK_BUTTONS_H */
