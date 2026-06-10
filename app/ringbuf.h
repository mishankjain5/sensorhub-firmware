#ifndef SENSORHUB_RINGBUF_H
#define SENSORHUB_RINGBUF_H

#include <stdbool.h>
#include <stdint.h>

/*
 * Fixed-capacity single-producer / single-consumer ring buffer.
 *
 * Designed for the classic firmware pattern: an interrupt handler (producer)
 * pushes items, the main loop (consumer) pops them. With one producer and one
 * consumer, and head/tail being aligned 16-bit values (atomic loads/stores on
 * Cortex-M), no locking or interrupt masking is needed:
 *   - push() only writes `head`, pop() only writes `tail`.
 *
 * Storage is caller-provided and fixed at init time — no heap, ever.
 * When full, push() drops the new item and counts it in `dropped` (losing
 * the newest sample is predictable; silently corrupting state is not).
 * One slot is intentionally left unused to distinguish full from empty.
 */

typedef int16_t rb_item_t;

typedef struct {
    rb_item_t *buf;
    uint16_t cap;                /* number of slots in buf */
    volatile uint16_t head;      /* next write index (producer-owned) */
    volatile uint16_t tail;      /* next read index (consumer-owned) */
    volatile uint32_t dropped;   /* pushes rejected because buffer was full */
} ringbuf_t;

void ringbuf_init(ringbuf_t *rb, rb_item_t *storage, uint16_t capacity);
bool ringbuf_push(ringbuf_t *rb, rb_item_t item); /* false + dropped++ if full */
bool ringbuf_pop(ringbuf_t *rb, rb_item_t *out);  /* false if empty */
bool ringbuf_is_empty(const ringbuf_t *rb);
uint16_t ringbuf_count(const ringbuf_t *rb);

#endif
