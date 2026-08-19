#include "mock_touch.h"

#include "janus_input_touch.h"

#define MOCK_TOUCH_QUEUE_CAPACITY 8

typedef struct { int16_t x, y; } mock_touch_point_t;
static mock_touch_point_t g_queue[MOCK_TOUCH_QUEUE_CAPACITY];
static uint8_t g_queue_count = 0;
static uint8_t g_queue_head = 0;

void mock_touch_reset(void) {
    g_queue_count = 0;
    g_queue_head = 0;
}

void mock_touch_queue_tap(int16_t x, int16_t y) {
    if (g_queue_count >= MOCK_TOUCH_QUEUE_CAPACITY) return;
    uint8_t tail = (uint8_t)((g_queue_head + g_queue_count) % MOCK_TOUCH_QUEUE_CAPACITY);
    g_queue[tail].x = x;
    g_queue[tail].y = y;
    g_queue_count++;
}

bool janus_touch_poll(int16_t *x, int16_t *y) {
    if (g_queue_count == 0) return false;
    *x = g_queue[g_queue_head].x;
    *y = g_queue[g_queue_head].y;
    g_queue_head = (uint8_t)((g_queue_head + 1) % MOCK_TOUCH_QUEUE_CAPACITY);
    g_queue_count--;
    return true;
}
