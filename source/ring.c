#include "../include/ring.h"

#include <string.h>

size_t ring_size(const struct Ring *ring)
{
    if (ring->begin == NULL) return 0;
    else if (ring->end >= ring->begin) return (size_t)(ring->end - ring->begin); /* No rollover */
    else return ring->buffer.size - (size_t)(ring->begin - ring->end); /* Rollover */
}

struct Error *ring_push(struct Ring *ring, size_t size)
{
    /* Pop aka advance 'end' */
    const size_t old_size = ring_size(ring);
    const size_t new_size = old_size + size;
    size_t remaining_window;
    if (new_size > ring->buffer.size)
    {
        const size_t old_buffer_size = ring->buffer.size;
        const char *old_p = ring->buffer.p;
        PRET(char_buffer_resize(&ring->buffer, new_size));
        ring->buffer.size = ring->buffer.capacity; /* Use call memory */
        ring->begin = ring->buffer.p + (ring->begin - old_p);
        ring->end = ring->buffer.p + (ring->end - old_p);
        if (ring->end < ring->begin) /* Rollover */
        {
            remaining_window = (size_t)(ring->buffer.p + ring->buffer.size - ring->begin);
            memmove(ring->buffer.p + ring->buffer.size - remaining_window, ring->buffer.p + old_buffer_size - remaining_window, remaining_window);
        }
    }
    /* The algorithm is over-engineered, but doesn't calculate invalid pointers */
    remaining_window = (size_t)(ring->buffer.p + ring->buffer.size - ring->end);
    if (remaining_window >= size) ring->end += size; /* No rollover */
    else ring->end = ring->buffer.p + (size - remaining_window); /* Rollover */
    return OK;
}

struct Error *ring_pop(struct Ring *ring, size_t size)
{
    /* Pop aka advance 'begin' */
    const size_t old_size = ring_size(ring);
    size_t remaining_window;
    ARET2(size <= old_size, "Cannot pop %u bytes from ring with size %u", (unsigned int)size, (unsigned int)old_size);
    /* The algorithm is over-engineered, but doesn't calculate invalid pointers */
    remaining_window = (size_t)(ring->buffer.p + ring->buffer.size - ring->begin);
    if (remaining_window >= size) ring->begin += size; /* No rollover */
    else ring->begin = ring->buffer.p + (size - remaining_window); /* Rollover */
    return OK;
}

struct Error *ring_get(const struct Ring *ring, size_t size, struct RingConstWindow windows[2], unsigned char *window_number)
{
    const size_t old_size = ring_size(ring);
    size_t remaining_window;
    ARET2(size <= old_size, "Cannot get %u bytes from ring with size %u", (unsigned int)size, (unsigned int)old_size);
    if (ring->begin == NULL)
    {
        *window_number = 0;
        return OK;
    }

    remaining_window = (size_t)(ring->buffer.p + ring->buffer.size - ring->begin);
    if (remaining_window >= size) /* No rollover */
    {
        *window_number = 1;
        windows[0].p = ring->begin;
        windows[0].size = size;
    }
    else /* Rollover */
    {
        *window_number = 2;
        windows[0].p = ring->begin;
        windows[0].size = remaining_window;
        windows[1].p = ring->buffer.p;
        windows[1].size = size - remaining_window;
    }
    return OK;
}

struct Error *ring_get_last(const struct Ring *ring, size_t size, struct RingConstWindow windows[2], unsigned char *window_number)
{
    const size_t old_size = ring_size(ring);
    size_t remaining_window;
    ARET2(size <= old_size, "Cannot get %u bytes from ring with size %u", (unsigned int)size, (unsigned int)old_size);
    if (ring->begin == NULL)
    {
        *window_number = 0;
        return OK;
    }

    remaining_window = (size_t)(ring->end - ring->buffer.p);
    if (remaining_window >= size) /* No backwards rollover */
    {
        *window_number = 1;
        windows[0].p = ring->end - size;
        windows[0].size = size;
    }
    else /* Backwards rollover */
    {
        *window_number = 2;
        windows[0].p = ring->buffer.p + ring->buffer.size - (size - remaining_window);
        windows[0].size = size - remaining_window;
        windows[1].p = ring->buffer.p;
        windows[1].size = size;
    }
    return OK;
}

void ring_get_all(const struct Ring *ring, struct RingConstWindow windows[2], unsigned char *window_number)
{
    if (ring->begin == NULL)
    {
        *window_number = 0;
    }
    else if (ring->end >= ring->begin) /* No rollover */
    {
        *window_number = 1;
        windows[0].p = ring->begin;
        windows[0].size = (size_t)(ring->end - ring->begin);
    }
    else /* Rollover */
    {
        *window_number = 2;
        windows[0].p = ring->begin;
        windows[0].size = ring->buffer.size - (size_t)(ring->begin - ring->buffer.p);
        windows[1].p = ring->buffer.p;
        windows[1].size = (size_t)(ring->end - ring->buffer.p);
    }
}

void ring_clear(struct Ring *ring)
{
    ring->begin = ring->buffer.p;
    ring->end = ring->buffer.p;
}