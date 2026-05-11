#include "../include/ring.h"
#include "../commonlib/include/error.h"
#include "../include/memory.h"
#include "../include/value.h"

#include <string.h>

void ring_finalize(struct Ring *ring)
{
    const struct Ring zero = ZERO_INIT;
    if (ring->p != NULL) count_free(ring->p, ring->capacity);
    *ring = zero;
}

struct Error *ring_reserve(struct Ring *ring, size_t capacity)
{
    size_t new_capacity;
    char *new_p;

    new_capacity = (ring->capacity == 0) ? 1 : ring->capacity;
    while (capacity > new_capacity) new_capacity *= 2;

    if (new_capacity == ring->capacity) return OK; /* Capacity is not changed */

    if (new_capacity < ring->capacity)
    {
        /* Shrinking */
        size_t continuous_after_begin;
        bool continuous_size_after_begin;
        ARET2(new_capacity >= ring->size, "Cannot shrink %u bytes into %u", (unsigned int)capacity, (unsigned int)ring->size);
        continuous_after_begin = ring->capacity - ring->begin;
        continuous_size_after_begin = ring->size <= continuous_after_begin;
        if (continuous_size_after_begin)
        {
            if (ring->begin + ring->size > new_capacity)
            {
                /* Relocating whole data to the beginning */
                memmove(ring->p, ring->p + ring->begin, ring->size);
                ring->begin = 0;
            }
        }
        else
        {
            /* Relocating first part to the new edge */
            const size_t shrink = ring->capacity - new_capacity;
            memmove(ring->p + ring->begin - shrink, ring->p + ring->begin, continuous_after_begin);
            ring->begin -= shrink;
        }
    }

    /* Reallocating */
    new_p = count_realloc(ring->p, ring->capacity, new_capacity);
    ARET(new_p != NULL);
    ring->p = new_p;

    if (new_capacity > ring->capacity)
    {
        /* Expanding */
        const size_t continuous_after_begin = ring->capacity - ring->begin;
        const bool continuous_size_after_begin = ring->size <= continuous_after_begin;
        if (!continuous_size_after_begin)
        {
            /* Relocating first part to the new edge */
            const size_t expand = new_capacity - ring->capacity;
            memmove(ring->p + ring->begin + expand, ring->p + ring->begin, continuous_after_begin);
            ring->begin += expand;
        }
    }

    ring->capacity = new_capacity;
    return OK;
}

void ring_get_all(const struct Ring *ring, struct Value *value)
{
    const struct Value zero = ZERO_INIT;
    size_t continuous_after_begin;
    bool continuous_size_after_begin;
    *value = zero;
    if (ring->p == NULL) return;
    
    continuous_after_begin = ring->capacity - ring->begin;
    continuous_size_after_begin = ring->size <= continuous_after_begin;
    if (continuous_size_after_begin)
    {
        value->parts[0].size = ring->size;
        value->parts[0].p = ring->p + ring->begin;
    }
    else
    {
        value->parts[0].size = continuous_after_begin;
        value->parts[0].p = ring->p + ring->begin;
        value->parts[1].size = ring->size - continuous_after_begin;
        value->parts[1].p = ring->p;
    }
}

struct Error *ring_get(const struct Ring *ring, const struct ValueLocation *location, bool latest, struct Value *value)
{
    const struct Value zero = ZERO_INIT;
    size_t offset;
    size_t continuous_after_begin;
    bool continuous_offset_after_begin;
    ARET2(location->offset + location->size <= ring->size, "Cannot get %u bytes from ring with size %u", (unsigned int)location->size, (unsigned int)ring->size);
    *value = zero;
    if (location->size == 0) return OK;

    offset = latest ? (ring->size - location->size - location->offset) : location->offset; /* Just invert it, it's that simple for now */

    continuous_after_begin = ring->capacity - ring->begin;
    continuous_offset_after_begin = offset <= continuous_after_begin;
    if (continuous_offset_after_begin)
    {
        size_t continuous_after_offset;
        bool continuous_size_after_offset;
        continuous_after_offset = continuous_after_begin - offset;
        
        if (continuous_after_offset == 0) /* Special case for ring->begin + offset == ring->capacity */
        {
            value->parts[0].size = location->size;
            value->parts[0].p = ring->p;
            return OK;
        }
        continuous_size_after_offset = location->size <= continuous_after_offset;
        if (continuous_size_after_offset)
        {
            value->parts[0].size = location->size;
            value->parts[0].p = ring->p + ring->begin + offset;
        }
        else
        {
            value->parts[0].size = continuous_after_offset;
            value->parts[0].p = ring->p + ring->begin + offset;
            value->parts[1].size = location->size - continuous_after_offset;
            value->parts[1].p = ring->p;
        }
    }
    else
    {
        value->parts[0].size = location->size;
        value->parts[0].p = ring->p + (offset - continuous_after_begin);
        /* Guaranteed no second rollover */
    }
    return OK;
}

struct Error *ring_push(struct Ring *ring, size_t size)
{
    ARET2(ring->size + size <= ring->capacity, "Cannot push %u bytes to ring with capacity %u", (unsigned int)size, (unsigned int)ring->capacity);
    ring->size += size;
    return OK;
}

struct Error *ring_push_write(struct Ring *ring, size_t size, const char *p)
{
    struct Value value;
    PRET(ring_push_get(ring, size, &value));
    value_write(&value, p);
    return OK;
}

struct Error *ring_push_get(struct Ring *ring, size_t size, struct Value *value)
{
    struct ValueLocation location;
    PRET(ring_push(ring, size));
    location.offset = 0;
    location.size = size;
    PRET(ring_get(ring, &location, true, value));
    return OK;
}

struct Error *ring_pop(struct Ring *ring, size_t size)
{
    size_t continuous_after_begin;
    bool continuous_size_after_begin;
    ARET2(ring->size >= size, "Cannot pop %u bytes from ring with size %u", (unsigned int)size, (unsigned int)ring->size);
    continuous_after_begin = ring->capacity - ring->begin;
    continuous_size_after_begin = size <= continuous_after_begin;
    ring->begin = continuous_size_after_begin ? (ring->begin + size) : (size - continuous_after_begin);
    ring->size -= size;
    if (ring->size == 0) ring->begin = 0; /* Move to begin because why not */
    return OK;
}

struct Error *ring_pop_read(struct Ring *ring, size_t size, char *p)
{
    struct ValueLocation location; struct Value value;
    location.offset = 0;
    location.size = size;
    PRET(ring_get(ring, &location, false, &value));
    value_read(&value, p);
    PRET(ring_pop(ring, size));
    return OK;
}

struct Error *ring_unpush(struct Ring *ring, size_t size)
{
    ARET2(ring->size >= size, "Cannot unpush %u bytes from ring with size %u", (unsigned int)size, (unsigned int)ring->size);
    ring->size -= size;
    return OK;
}
