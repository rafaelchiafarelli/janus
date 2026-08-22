/* Host-side display driver for testing/dev — records every draw call
 * instead of touching real hardware. This is the "host mock driver for
 * testing" the original prototype's own DESIGN.md promised but never
 * delivered (Janus.md). Dev/test tooling only, not part of the shipped
 * fixed runtime library.
 */
#ifndef JANUS_HOST_MOCK_DRIVER_H
#define JANUS_HOST_MOCK_DRIVER_H

#include <stdint.h>

#define MOCK_DRIVER_LOG_CAPACITY 256

typedef struct {
    uint16_t x, y, w, h;
    uint16_t sample_pixel;   /* first RGB565 pixel of that call's buffer —
                               * enough to tell fill colors apart in tests */
} mock_draw_call_t;

extern mock_draw_call_t mock_driver_log[MOCK_DRIVER_LOG_CAPACITY];
extern uint16_t mock_driver_log_count;

void mock_driver_reset(void);

/* Test control for display_busy() — makes the next `count` calls to
 * display_busy() return true, then false again. Lets tests exercise the
 * non-blocking render path's backoff (janus_render_poll must not advance
 * while busy) without any real hardware/DMA involved. */
void mock_driver_set_busy_for(uint16_t count);

#endif /* JANUS_HOST_MOCK_DRIVER_H */
