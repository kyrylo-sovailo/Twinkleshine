#ifndef RING_H
#define RING_H

#include "../commonlib/include/bool.h"
#include "../commonlib/include/macro.h"

#include <stddef.h>

struct Error;
struct Value;
struct ValueLocation;

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

/* Grants access to all available bytes */
void ring_get_all(const struct Ring *ring, struct Value *value);

/* Grants access to arbitrary bytes */
struct Error *ring_get(const struct Ring *ring, const struct ValueLocation *location, bool last, struct Value *value) NODISCARD;

/* Adds first N unused bytes to ring, then optionally populate/get */
struct Error *ring_push(struct Ring *ring, size_t size) NODISCARD;
struct Error *ring_push_write(struct Ring *ring, size_t size, const char *p) NODISCARD;
struct Error *ring_push_get(struct Ring *ring, size_t size, struct Value *value) NODISCARD;

/* Removes first N used bytes, before that optionally read */
struct Error *ring_pop(struct Ring *ring, size_t size) NODISCARD;
struct Error *ring_pop_read(struct Ring *ring, size_t size, char *p) NODISCARD;

/* Removes last N used bytes */
struct Error *ring_unpush(struct Ring *ring, size_t size) NODISCARD;

#endif
