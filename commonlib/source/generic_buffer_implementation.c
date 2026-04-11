#include "../include/buffer_implementation.h"
#include "../../include/memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

DECLARE_BUFFER(GENERIC_ARGMENT_1, GenericByteBuffer)
DECLARE_BUFFER(GENERIC_ARGMENT_2, GenericWordBuffer)
DECLARE_BUFFER(GENERIC_ARGMENT_4, GenericDWordBuffer)
DECLARE_BUFFER(GENERIC_ARGMENT_8, GenericQWordBuffer)

void generic_buffer_initialize(void *buffer)
{
    const struct GenericByteBuffer zero = ZERO_INIT;
    struct GenericByteBuffer* cast = (struct GenericByteBuffer*)buffer;
    *cast = zero;
}

#define IMPLEMENT_GENERIC_BUFFER_FINALIZE(TYPE, STRUCT_NAME, SIZE_EXPRESSION) \
{ \
    const struct STRUCT_NAME zero = ZERO_INIT; \
    struct STRUCT_NAME* cast = (struct STRUCT_NAME*)buffer; \
    if (cast->p != NULL) count_free(cast->p, cast->capacity * SIZE_EXPRESSION); \
    *cast = zero; \
}
void generic_buffer_finalize_1(void *buffer)
IMPLEMENT_GENERIC_BUFFER_FINALIZE(GENERIC_ARGMENT_1, GenericByteBuffer, sizeof(*cast->p))
void generic_buffer_finalize_2(void *buffer)
IMPLEMENT_GENERIC_BUFFER_FINALIZE(GENERIC_ARGMENT_2, GenericWordBuffer, sizeof(*cast->p))
void generic_buffer_finalize_4(void *buffer)
IMPLEMENT_GENERIC_BUFFER_FINALIZE(GENERIC_ARGMENT_4, GenericDWordBuffer, sizeof(*cast->p))
void generic_buffer_finalize_8(void *buffer)
IMPLEMENT_GENERIC_BUFFER_FINALIZE(GENERIC_ARGMENT_8, GenericQWordBuffer, sizeof(*cast->p))
void generic_buffer_finalize_n(void *buffer, size_t element_sizeof)
IMPLEMENT_GENERIC_BUFFER_FINALIZE(GENERIC_ARGMENT_1, GenericByteBuffer, element_sizeof)

#define IMPLEMENT_GENERIC_BUFFER_RESIZE(TYPE, STRUCT_NAME, SIZE_EXPRESSION) \
{ \
    struct STRUCT_NAME* cast = (struct STRUCT_NAME*)buffer; \
    if (size > cast->capacity) \
    { \
        TYPE *new_p; \
        size_t new_capacity = (cast->capacity == 0) ? 1 : cast->capacity; \
        while (size > new_capacity) new_capacity *= 2; \
        new_p = (TYPE*)count_realloc(cast->p, cast->capacity * SIZE_EXPRESSION, new_capacity * SIZE_EXPRESSION); \
        ARET(new_p != NULL); \
        cast->capacity = new_capacity; \
        cast->p = new_p; \
    } \
    cast->size = size; \
    ERROR_RETURN_OK(); \
}
ERROR_TYPE generic_buffer_resize_1(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESIZE(GENERIC_ARGMENT_1, GenericByteBuffer, sizeof(*cast->p))
ERROR_TYPE generic_buffer_resize_2(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESIZE(GENERIC_ARGMENT_2, GenericWordBuffer, sizeof(*cast->p))
ERROR_TYPE generic_buffer_resize_4(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESIZE(GENERIC_ARGMENT_4, GenericDWordBuffer, sizeof(*cast->p))
ERROR_TYPE generic_buffer_resize_8(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESIZE(GENERIC_ARGMENT_8, GenericQWordBuffer, sizeof(*cast->p))
ERROR_TYPE generic_buffer_resize_n(void *buffer, size_t size, size_t element_sizeof)
IMPLEMENT_GENERIC_BUFFER_RESIZE(GENERIC_ARGMENT_1, GenericByteBuffer, element_sizeof)

#define IMPLEMENT_GENERIC_BUFFER_RESERVE(TYPE, STRUCT_NAME, SIZE_EXPRESSION) \
{ \
    struct STRUCT_NAME* cast = (struct STRUCT_NAME*)buffer; \
    if (size > cast->capacity) \
    { \
        TYPE *new_p; \
        size_t new_capacity = (cast->capacity == 0) ? 1 : cast->capacity; \
        while (size > new_capacity) new_capacity *= 2; \
        new_p = (TYPE*)count_realloc(cast->p, cast->capacity * SIZE_EXPRESSION, new_capacity * SIZE_EXPRESSION); \
        ARET(new_p != NULL); \
        cast->capacity = new_capacity; \
        cast->p = new_p; \
    } \
    cast->size = size; \
    ERROR_RETURN_OK(); \
}
ERROR_TYPE generic_buffer_reserve_1(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESERVE(GENERIC_ARGMENT_1, GenericByteBuffer, sizeof(*cast->p))
ERROR_TYPE generic_buffer_reserve_2(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESERVE(GENERIC_ARGMENT_2, GenericWordBuffer, sizeof(*cast->p))
ERROR_TYPE generic_buffer_reserve_4(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESERVE(GENERIC_ARGMENT_4, GenericDWordBuffer, sizeof(*cast->p))
ERROR_TYPE generic_buffer_reserve_8(void *buffer, size_t size)
IMPLEMENT_GENERIC_BUFFER_RESERVE(GENERIC_ARGMENT_8, GenericQWordBuffer, sizeof(*cast->p))
ERROR_TYPE generic_buffer_reserve_n(void *buffer, size_t size, size_t element_sizeof)
IMPLEMENT_GENERIC_BUFFER_RESERVE(GENERIC_ARGMENT_1, GenericByteBuffer, element_sizeof)

#define IMPLEMENT_GENERIC_BUFFER_APPEND(TYPE, STRUCT_NAME, SIZE_EXPRESSION, OLD_SIZE_POINTER_EXPRESSION) \
{ \
    struct STRUCT_NAME* cast = (struct STRUCT_NAME*)buffer; \
    if (cast->size + size > cast->capacity) \
    { \
        TYPE *new_p; \
        size_t new_capacity = (cast->capacity == 0) ? 1 : cast->capacity; \
        while (cast->size + size > new_capacity) new_capacity *= 2; \
        new_p = (TYPE*)count_realloc(cast->p, cast->capacity * SIZE_EXPRESSION, new_capacity * SIZE_EXPRESSION); \
        ARET(new_p != NULL); \
        cast->capacity = new_capacity; \
        cast->p = new_p; \
    } \
    memcpy(OLD_SIZE_POINTER_EXPRESSION, data, size * SIZE_EXPRESSION); \
    cast->size += size; \
    ERROR_RETURN_OK(); \
}
ERROR_TYPE generic_buffer_append_1(void *buffer, const void *data, size_t size)
IMPLEMENT_GENERIC_BUFFER_APPEND(GENERIC_ARGMENT_1, GenericByteBuffer, sizeof(*cast->p), cast->p + cast->size)
ERROR_TYPE generic_buffer_append_2(void *buffer, const void *data, size_t size)
IMPLEMENT_GENERIC_BUFFER_APPEND(GENERIC_ARGMENT_2, GenericWordBuffer, sizeof(*cast->p), cast->p + cast->size)
ERROR_TYPE generic_buffer_append_4(void *buffer, const void *data, size_t size)
IMPLEMENT_GENERIC_BUFFER_APPEND(GENERIC_ARGMENT_4, GenericDWordBuffer, sizeof(*cast->p), cast->p + cast->size)
ERROR_TYPE generic_buffer_append_8(void *buffer, const void *data, size_t size)
IMPLEMENT_GENERIC_BUFFER_APPEND(GENERIC_ARGMENT_8, GenericQWordBuffer, sizeof(*cast->p), cast->p + cast->size)
ERROR_TYPE generic_buffer_append_n(void *buffer, const void *data, size_t size, size_t element_sizeof)
IMPLEMENT_GENERIC_BUFFER_APPEND(GENERIC_ARGMENT_1, GenericByteBuffer, element_sizeof, cast->p + cast->size * element_sizeof)

#define IMPLEMENT_GENERIC_BUFFER_PUSH(TYPE, STRUCT_NAME, SIZE_EXPRESSION) \
{ \
    struct STRUCT_NAME* cast = (struct STRUCT_NAME*)buffer; \
    if (cast->size + 1 > cast->capacity) \
    { \
        const size_t new_capacity = (cast->capacity == 0) ? 1 : (cast->capacity * 2); \
        TYPE *new_p = (TYPE*)count_realloc(cast->p, cast->capacity * SIZE_EXPRESSION, new_capacity * SIZE_EXPRESSION); \
        ARET(new_p != NULL); \
        cast->capacity = new_capacity; \
        cast->p = new_p; \
    } \
    cast->p[cast->size] = data; \
    cast->size += 1; \
    ERROR_RETURN_OK(); \
}
ERROR_TYPE generic_buffer_push_1(void *buffer, GENERIC_ARGMENT_1 data)
IMPLEMENT_GENERIC_BUFFER_PUSH(GENERIC_ARGMENT_1, GenericByteBuffer, sizeof(*cast->p))
ERROR_TYPE generic_buffer_push_2(void *buffer, GENERIC_ARGMENT_2 data)
IMPLEMENT_GENERIC_BUFFER_PUSH(GENERIC_ARGMENT_2, GenericWordBuffer, sizeof(*cast->p))
ERROR_TYPE generic_buffer_push_4(void *buffer, GENERIC_ARGMENT_4 data)
IMPLEMENT_GENERIC_BUFFER_PUSH(GENERIC_ARGMENT_4, GenericDWordBuffer, sizeof(*cast->p))
ERROR_TYPE generic_buffer_push_8(void *buffer, GENERIC_ARGMENT_8 data)
IMPLEMENT_GENERIC_BUFFER_PUSH(GENERIC_ARGMENT_8, GenericQWordBuffer, sizeof(*cast->p))
