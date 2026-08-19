#include "mock_driver.h"

#include <stddef.h>

#include "janus_runtime.h"

mock_draw_call_t mock_driver_log[MOCK_DRIVER_LOG_CAPACITY];
uint16_t mock_driver_log_count = 0;

void mock_driver_reset(void) {
    mock_driver_log_count = 0;
}

static void record(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *pixels) {
    if (mock_driver_log_count >= MOCK_DRIVER_LOG_CAPACITY) return;
    mock_draw_call_t *entry = &mock_driver_log[mock_driver_log_count++];
    entry->x = x;
    entry->y = y;
    entry->w = w;
    entry->h = h;
    entry->sample_byte = (pixels != NULL) ? pixels[0] : 0;
}

void draw_area_sync(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *pixels) {
    record(x, y, w, h, pixels);
}

bool draw_area_async(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *pixels) {
    record(x, y, w, h, pixels);
    return true; /* accepted immediately — no real non-blocking scheduling yet */
}

bool display_busy(void) {
    return false;
}
