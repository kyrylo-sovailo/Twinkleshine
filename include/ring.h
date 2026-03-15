#ifndef RING_H
#define RING_H

#include "error.h"
#include "value.h"

#include <stddef.h>

struct Ring
{
    char *p;
    size_t size;
    size_t capacity;
    size_t begin;
};

/* Finalizes the ring */
void ring_finalize(struct Ring *ring);

/* Changes ring capacity (and frees excess memory, unlike a buffer) */
struct Error *ring_reserve(struct Ring *ring, size_t capacity);

/* Adds first N unused bytes to ring, optionally populate */
struct Error *ring_push(struct Ring *ring, size_t size, const char *p) NODISCARD;

/* Removes last N used bytes to ring */
struct Error *ring_unpush(struct Ring *ring, size_t size) NODISCARD;

/* Removes first N used bytes */
struct Error *ring_pop(struct Ring *ring, size_t size) NODISCARD;

/* Grants access to all available bytes */
void ring_get_all(const struct Ring *ring, struct Value *value);

/* Grants access to arbitrary bytes */
struct Error *ring_get(const struct Ring *ring, const struct ValueLocation *location, bool last, struct Value *value);

#endif
