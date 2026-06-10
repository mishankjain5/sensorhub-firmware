#include "ringbuf.h"

void ringbuf_init(ringbuf_t *rb, rb_item_t *storage, uint16_t capacity)
{
    rb->buf = storage;
    rb->cap = capacity;
    rb->head = 0;
    rb->tail = 0;
    rb->dropped = 0;
}

static uint16_t next_index(const ringbuf_t *rb, uint16_t i)
{
    i++;
    if (i == rb->cap) {
        i = 0;
    }
    return i;
}

bool ringbuf_push(ringbuf_t *rb, rb_item_t item)
{
    uint16_t head = rb->head;
    uint16_t next = next_index(rb, head);

    if (next == rb->tail) {      /* full: head would catch up to tail */
        rb->dropped++;
        return false;
    }
    rb->buf[head] = item;        /* write the slot first ...           */
    rb->head = next;             /* ... then publish it to the consumer */
    return true;
}

bool ringbuf_pop(ringbuf_t *rb, rb_item_t *out)
{
    uint16_t tail = rb->tail;

    if (tail == rb->head) {      /* empty */
        return false;
    }
    *out = rb->buf[tail];
    rb->tail = next_index(rb, tail);
    return true;
}

bool ringbuf_is_empty(const ringbuf_t *rb)
{
    return rb->head == rb->tail;
}

uint16_t ringbuf_count(const ringbuf_t *rb)
{
    uint16_t head = rb->head;
    uint16_t tail = rb->tail;

    if (head >= tail) {
        return (uint16_t)(head - tail);
    }
    return (uint16_t)(rb->cap - tail + head);
}
