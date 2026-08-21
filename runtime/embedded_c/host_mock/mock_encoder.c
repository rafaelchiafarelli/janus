#include "mock_encoder.h"

#include "janus_input_encoder.h"

#define MOCK_ENCODER_QUEUE_CAPACITY 8

typedef struct { janus_encoder_event_t event; int16_t delta; } mock_encoder_event_t;
static mock_encoder_event_t g_queue[MOCK_ENCODER_QUEUE_CAPACITY];
static uint8_t g_queue_count = 0;
static uint8_t g_queue_head = 0;

static void enqueue(janus_encoder_event_t event, int16_t delta) {
    if (g_queue_count >= MOCK_ENCODER_QUEUE_CAPACITY) return;
    uint8_t tail = (uint8_t)((g_queue_head + g_queue_count) % MOCK_ENCODER_QUEUE_CAPACITY);
    g_queue[tail].event = event;
    g_queue[tail].delta = delta;
    g_queue_count++;
}

void mock_encoder_reset(void) {
    g_queue_count = 0;
    g_queue_head = 0;
}

void mock_encoder_queue_rotate(int16_t delta) {
    enqueue(JANUS_ENCODER_ROTATE, delta);
}

void mock_encoder_queue_click(void) {
    enqueue(JANUS_ENCODER_CLICK, 0);
}

bool janus_encoder_poll(janus_encoder_event_t *event, int16_t *delta) {
    if (g_queue_count == 0) return false;
    *event = g_queue[g_queue_head].event;
    *delta = g_queue[g_queue_head].delta;
    g_queue_head = (uint8_t)((g_queue_head + 1) % MOCK_ENCODER_QUEUE_CAPACITY);
    g_queue_count--;
    return true;
}
