#ifndef BUFFER_IMPLEMENTATION_H
#define BUFFER_IMPLEMENTATION_H

#include "buffer.h"

#define IMPLEMENT_BUFFER_FINALIZE(TYPE, STRUCT_NAME, FUNCTION_NAME) \
void FUNCTION_NAME ## finalize(struct STRUCT_NAME *buffer) \
{ \
    generic_buffer_finalize(buffer); \
}
#define IMPLEMENT_BUFFER_RESIZE(TYPE, STRUCT_NAME, FUNCTION_NAME) \
struct Error *FUNCTION_NAME ## resize(struct STRUCT_NAME *buffer, size_t size) \
{ \
    switch (sizeof(*buffer->p)) \
    { \
    case 1: return generic_buffer_resize_1(buffer, size); \
    case 2: return generic_buffer_resize_2(buffer, size); \
    case 4: return generic_buffer_resize_4(buffer, size); \
    case 8: return generic_buffer_resize_8(buffer, size); \
    default: return generic_buffer_resize_n(buffer, size, sizeof(*buffer->p)); \
    } \
}
#define IMPLEMENT_BUFFER_APPEND(TYPE, STRUCT_NAME, FUNCTION_NAME) \
struct Error *FUNCTION_NAME ## append(struct STRUCT_NAME *buffer, const TYPE *data, size_t size) \
{ \
    switch (sizeof(*buffer->p)) \
    { \
    case 1: return generic_buffer_append_1(buffer, data, size); \
    case 2: return generic_buffer_append_2(buffer, data, size); \
    case 4: return generic_buffer_append_4(buffer, data, size); \
    case 8: return generic_buffer_append_8(buffer, data, size); \
    default: return generic_buffer_append_n(buffer, data, size, sizeof(*buffer->p)); \
    } \
}
#define IMPLEMENT_BUFFER_PUSH(TYPE, STRUCT_NAME, FUNCTION_NAME, SIZE) \
struct Error *FUNCTION_NAME ## push(struct STRUCT_NAME *buffer, TYPE data) \
{ \
    struct Error *check[(sizeof(*buffer->p) == SIZE && sizeof(data) == SIZE) ? 1 : -1]; \
    check[0] = generic_buffer_push_ ## SIZE(buffer, (GENERIC_ARGMENT_ ## SIZE)data); \
    return check[0]; \
}

void generic_buffer_finalize(void *buffer);

struct Error *generic_buffer_resize_1(void *buffer, size_t size);
struct Error *generic_buffer_resize_2(void *buffer, size_t size);
struct Error *generic_buffer_resize_4(void *buffer, size_t size);
struct Error *generic_buffer_resize_8(void *buffer, size_t size);
struct Error *generic_buffer_resize_n(void *buffer, size_t size, size_t element_sizeof);

struct Error *generic_buffer_reserve_1(void *buffer, size_t size);
struct Error *generic_buffer_reserve_2(void *buffer, size_t size);
struct Error *generic_buffer_reserve_4(void *buffer, size_t size);
struct Error *generic_buffer_reserve_8(void *buffer, size_t size);
struct Error *generic_buffer_reserve_n(void *buffer, size_t size, size_t element_sizeof);

struct Error *generic_buffer_append_1(void *buffer, const void *data, size_t size);
struct Error *generic_buffer_append_2(void *buffer, const void *data, size_t size);
struct Error *generic_buffer_append_4(void *buffer, const void *data, size_t size);
struct Error *generic_buffer_append_8(void *buffer, const void *data, size_t size);
struct Error *generic_buffer_append_n(void *buffer, const void *data, size_t size, size_t element_sizeof);

struct Error *generic_buffer_push_1(void *buffer, GENERIC_ARGMENT_1 data);
struct Error *generic_buffer_push_2(void *buffer, GENERIC_ARGMENT_2 data);
struct Error *generic_buffer_push_4(void *buffer, GENERIC_ARGMENT_4 data);
struct Error *generic_buffer_push_8(void *buffer, GENERIC_ARGMENT_8 data);

#endif
