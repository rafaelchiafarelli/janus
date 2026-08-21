#include "mock_buttons.h"

#define MOCK_BUTTONS_QUEUE_CAPACITY 8

static janus_button_event_t g_queue[MOCK_BUTTONS_QUEUE_CAPACITY];
static uint8_t g_queue_count = 0;
static uint8_t g_queue_head = 0;

void mock_buttons_reset(void) {
    g_queue_count = 0;
    g_queue_head = 0;
}

void mock_buttons_queue(janus_button_event_t event) {
    if (g_queue_count >= MOCK_BUTTONS_QUEUE_CAPACITY) return;
    uint8_t tail = (uint8_t)((g_queue_head + g_queue_count) % MOCK_BUTTONS_QUEUE_CAPACITY);
    g_queue[tail] = event;
    g_queue_count++;
}

bool janus_buttons_poll(janus_button_event_t *event) {
    if (g_queue_count == 0) return false;
    *event = g_queue[g_queue_head];
    g_queue_head = (uint8_t)((g_queue_head + 1) % MOCK_BUTTONS_QUEUE_CAPACITY);
    g_queue_count--;
    return true;
}
