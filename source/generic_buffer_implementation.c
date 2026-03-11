#include "../include/buffer_implementation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DECLARE_BUFFER(GENERIC_ARGMENT_1, GenericByteBuffer)
DECLARE_BUFFER(GENERIC_ARGMENT_2, GenericWordBuffer)
DECLARE_BUFFER(GENERIC_ARGMENT_4, GenericDWordBuffer)
DECLARE_BUFFER(GENERIC_ARGMENT_8, GenericQWordBuffer)

#define IMPLEMENT_GENERIC_BUFFER_RESIZE(STRUCT_NAME, SIZE_EXPRESSION) \
{ \
    struct STRUCT_NAME* cast = (struct STRUCT_NAME*)buffer; \
    if (size > cast->capacity) \
    { \
        void *new_p; \
        size_t new_capacity = (cast->capacity == 0) ? 1 : cast->capacity; \
        while (size > new_capacity) new_capacity *= 2; \
        new_p = realloc(cast->p, new_capacity * SIZE_EXPRESSION); \
        ARET(new_p != NULL); \
        cast->capacity = new_capacity; \
        cast->p = new_p; \
    } \
    cast->size = size; \
    return OK; \
}

#define IMPLEMENT_GENERIC_BUFFER_RESERVE(STRUCT_NAME, SIZE_EXPRESSION) \
{ \
    struct STRUCT_NAME* cast = (struct STRUCT_NAME*)buffer; \
    if (size > cast->capacity) \
    { \
        void *new_p; \
        size_t new_capacity = (cast->capacity == 0) ? 1 : cast->capacity; \
        while (size > new_capacity) new_capacity *= 2; \
        new_p = realloc(cast->p, new_capacity * SIZE_EXPRESSION); \
        ARET(new_p != NULL); \
        cast->capacity = new_capacity; \
        cast->p = new_p; \
    } \
    cast->size = size; \
    return OK; \
}

#define IMPLEMENT_GENERIC_BUFFER_APPEND(STRUCT_NAME, SIZE_EXPRESSION, OLD_SIZE_POINTER_EXPRESSION) \
{ \
    struct STRUCT_NAME* cast = (struct STRUCT_NAME*)buffer; \
    if (cast->size + size > cast->capacity) \
    { \
        void *new_p; \
        size_t new_capacity = (cast->capacity == 0) ? 1 : cast->capacity; \
        while (cast->size + size > new_capacity) new_capacity *= 2; \
        new_p = realloc(cast->p, new_capacity * SIZE_EXPRESSION); \
        ARET(new_p != NULL); \
        cast->capacity = new_capacity; \
        cast->p = new_p; \
    } \
    memcpy(OLD_SIZE_POINTER_EXPRESSION, data, size * SIZE_EXPRESSION); \
    cast->size += size; \
    return OK; \
}

#define IMPLEMENT_GENERIC_BUFFER_PUSH(STRUCT_NAME, SIZE_EXPRESSION) \
{ \
    struct STRUCT_NAME* cast = (struct STRUCT_NAME*)buffer; \
    if (cast->size + 1 > cast->capacity) \
    { \
        const size_t new_capacity = (cast->capacity == 0) ? 1 : (cast->capacity << 1); \
        void *new_p = realloc(cast->p, new_capacity * SIZE_EXPRESSION); \
        ARET(new_p != NULL); \
        cast->capacity = new_capacity; \
        cast->p = new_p; \
    } \
    cast->p[cast->size] = data; \
    cast->size += 1; \
    return OK; \
}

void generic_buffer_finalize(void *buffer)
{
    struct GenericByteBuffer* cast = (struct GenericByteBuffer*)buffer;
    if (cast->p != NULL) free(cast->p);
    memset(cast, 0, sizeof(*cast));
}

struct Error *generic_buffer_resize_1(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESIZE(GenericByteBuffer, sizeof(*cast->p))
struct Error *generic_buffer_resize_2(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESIZE(GenericWordBuffer, sizeof(*cast->p))
struct Error *generic_buffer_resize_4(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESIZE(GenericDWordBuffer, sizeof(*cast->p))
struct Error *generic_buffer_resize_8(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESIZE(GenericQWordBuffer, sizeof(*cast->p))
struct Error *generic_buffer_resize_n(void *buffer, size_t size, size_t element_sizeof)
IMPLEMENT_GENERIC_BUFFER_RESIZE(GenericByteBuffer, element_sizeof)

struct Error *generic_buffer_reserve_1(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESERVE(GenericByteBuffer, sizeof(*cast->p))
struct Error *generic_buffer_reserve_2(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESERVE(GenericWordBuffer, sizeof(*cast->p))
struct Error *generic_buffer_reserve_4(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESERVE(GenericDWordBuffer, sizeof(*cast->p))
struct Error *generic_buffer_reserve_8(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESERVE(GenericQWordBuffer, sizeof(*cast->p))
struct Error *generic_buffer_reserve_n(void *buffer, size_t size, size_t element_sizeof)
IMPLEMENT_GENERIC_BUFFER_RESERVE(GenericByteBuffer, element_sizeof)

struct Error *generic_buffer_append_1(void *buffer, const void *data, size_t size)
IMPLEMENT_GENERIC_BUFFER_APPEND(GenericByteBuffer, sizeof(*cast->p), cast->p + cast->size)
struct Error *generic_buffer_append_2(void *buffer, const void *data, size_t size)
IMPLEMENT_GENERIC_BUFFER_APPEND(GenericWordBuffer, sizeof(*cast->p), cast->p + cast->size)
struct Error *generic_buffer_append_4(void *buffer, const void *data, size_t size)
IMPLEMENT_GENERIC_BUFFER_APPEND(GenericDWordBuffer, sizeof(*cast->p), cast->p + cast->size)
struct Error *generic_buffer_append_8(void *buffer, const void *data, size_t size)
IMPLEMENT_GENERIC_BUFFER_APPEND(GenericQWordBuffer, sizeof(*cast->p), cast->p + cast->size)
struct Error *generic_buffer_append_n(void *buffer, const void *data, size_t size, size_t element_sizeof)
IMPLEMENT_GENERIC_BUFFER_APPEND(GenericByteBuffer, element_sizeof, cast->p + cast->size * element_sizeof)

struct Error *generic_buffer_push_1(void *buffer, GENERIC_ARGMENT_1 data)
IMPLEMENT_GENERIC_BUFFER_PUSH(GenericByteBuffer, sizeof(*cast->p))
struct Error *generic_buffer_push_2(void *buffer, GENERIC_ARGMENT_2 data)
IMPLEMENT_GENERIC_BUFFER_PUSH(GenericWordBuffer, sizeof(*cast->p))
struct Error *generic_buffer_push_4(void *buffer, GENERIC_ARGMENT_4 data)
IMPLEMENT_GENERIC_BUFFER_PUSH(GenericDWordBuffer, sizeof(*cast->p))
struct Error *generic_buffer_push_8(void *buffer, GENERIC_ARGMENT_8 data)
IMPLEMENT_GENERIC_BUFFER_PUSH(GenericQWordBuffer, sizeof(*cast->p))
