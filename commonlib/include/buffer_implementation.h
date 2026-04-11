#ifndef COMMONLIB_BUFFER_IMPLEMENTATION_H
#define COMMONLIB_BUFFER_IMPLEMENTATION_H

#include "buffer.h"

#define IMPLEMENT_BUFFER_INITIALIZE(TYPE, STRUCT_NAME, FUNCTION_NAME) \
void FUNCTION_NAME ## initialize(struct STRUCT_NAME *buffer) \
{ \
    generic_buffer_initialize(buffer); \
}

#define IMPLEMENT_BUFFER_FINALIZE(TYPE, STRUCT_NAME, FUNCTION_NAME) \
void FUNCTION_NAME ## finalize(struct STRUCT_NAME *buffer) \
{ \
    TYPE *p; \
    for (p = buffer->p; p < buffer->p + buffer->size; p++) FUNCTION_NAME ## finalize_element(p); \
    switch (sizeof(*buffer->p)) \
    { \
    case 1: generic_buffer_finalize_1(buffer); break; \
    case 2: generic_buffer_finalize_2(buffer); break; \
    case 4: generic_buffer_finalize_4(buffer); break; \
    case 8: generic_buffer_finalize_8(buffer); break; \
    default: generic_buffer_finalize_n(buffer, sizeof(*buffer->p)); break; \
    } \
}

#define IMPLEMENT_BUFFER_RESIZE(TYPE, STRUCT_NAME, FUNCTION_NAME) \
ERROR_TYPE FUNCTION_NAME ## resize(struct STRUCT_NAME *buffer, size_t size) \
{ \
    const size_t old_size = buffer->size; \
    TYPE *p; \
    ERROR_DECLARE(); \
    for (p = buffer->p + size; p < buffer->p + old_size; p++) FUNCTION_NAME ## finalize_element(p); \
    switch (sizeof(*buffer->p)) \
    { \
    case 1: ERROR_ASSIGN(generic_buffer_resize_1(buffer, size)); break; \
    case 2: ERROR_ASSIGN(generic_buffer_resize_2(buffer, size)); break; \
    case 4: ERROR_ASSIGN(generic_buffer_resize_4(buffer, size)); break; \
    case 8: ERROR_ASSIGN(generic_buffer_resize_8(buffer, size)); break; \
    default: ERROR_ASSIGN(generic_buffer_resize_n(buffer, size, sizeof(*buffer->p))); break; \
    } \
    for (p = buffer->p + old_size; p < buffer->p + size; p++) FUNCTION_NAME ## initialize_element(p); \
    ERROR_RETURN_VERBATIM(); \
}

#define IMPLEMENT_BUFFER_RESERVE(TYPE, STRUCT_NAME, FUNCTION_NAME) \
ERROR_TYPE FUNCTION_NAME ## reserve(struct STRUCT_NAME *buffer, size_t size) \
{ \
    ERROR_DECLARE(); \
    switch (sizeof(*buffer->p)) \
    { \
    case 1: ERROR_ASSIGN(generic_buffer_reserve_1(buffer, size)); break; \
    case 2: ERROR_ASSIGN(generic_buffer_reserve_2(buffer, size)); break; \
    case 4: ERROR_ASSIGN(generic_buffer_reserve_4(buffer, size)); break; \
    case 8: ERROR_ASSIGN(generic_buffer_reserve_8(buffer, size)); break; \
    default: ERROR_ASSIGN(generic_buffer_reserve_n(buffer, size, sizeof(*buffer->p))); break; \
    } \
    ERROR_RETURN_VERBATIM(); \
}

#define IMPLEMENT_BUFFER_APPEND(TYPE, STRUCT_NAME, FUNCTION_NAME) \
ERROR_TYPE FUNCTION_NAME ## append(struct STRUCT_NAME *buffer, const TYPE *data, size_t size) \
{ \
    ERROR_DECLARE(); \
    switch (sizeof(*buffer->p)) \
    { \
    case 1: ERROR_ASSIGN(generic_buffer_append_1(buffer, data, size)); break; \
    case 2: ERROR_ASSIGN(generic_buffer_append_2(buffer, data, size)); break; \
    case 4: ERROR_ASSIGN(generic_buffer_append_4(buffer, data, size)); break; \
    case 8: ERROR_ASSIGN(generic_buffer_append_8(buffer, data, size)); break; \
    default: ERROR_ASSIGN(generic_buffer_append_n(buffer, data, size, sizeof(*buffer->p))); break; \
    } \
    ERROR_RETURN_VERBATIM(); \
}

#define IMPLEMENT_BUFFER_PUSH(TYPE, STRUCT_NAME, FUNCTION_NAME, SIZE) \
ERROR_TYPE FUNCTION_NAME ## push(struct STRUCT_NAME *buffer, TYPE data) \
{ \
    bool static_check[(sizeof(*buffer->p) == SIZE && sizeof(data) == SIZE) ? 1 : -1]; \
    ERROR_DECLARE(); \
    (void)static_check; \
    ERROR_ASSIGN(generic_buffer_push_ ## SIZE(buffer, (GENERIC_ARGMENT_ ## SIZE)data)); \
    ERROR_RETURN_VERBATIM(); \
}

void generic_buffer_initialize(void *buffer);
void generic_buffer_finalize_1(void *buffer);
void generic_buffer_finalize_2(void *buffer);
void generic_buffer_finalize_4(void *buffer);
void generic_buffer_finalize_8(void *buffer);
void generic_buffer_finalize_n(void *buffer, size_t element_sizeof);

ERROR_TYPE generic_buffer_resize_1(void *buffer, size_t size);
ERROR_TYPE generic_buffer_resize_2(void *buffer, size_t size);
ERROR_TYPE generic_buffer_resize_4(void *buffer, size_t size);
ERROR_TYPE generic_buffer_resize_8(void *buffer, size_t size);
ERROR_TYPE generic_buffer_resize_n(void *buffer, size_t size, size_t element_sizeof);

ERROR_TYPE generic_buffer_reserve_1(void *buffer, size_t size);
ERROR_TYPE generic_buffer_reserve_2(void *buffer, size_t size);
ERROR_TYPE generic_buffer_reserve_4(void *buffer, size_t size);
ERROR_TYPE generic_buffer_reserve_8(void *buffer, size_t size);
ERROR_TYPE generic_buffer_reserve_n(void *buffer, size_t size, size_t element_sizeof);

ERROR_TYPE generic_buffer_append_1(void *buffer, const void *data, size_t size);
ERROR_TYPE generic_buffer_append_2(void *buffer, const void *data, size_t size);
ERROR_TYPE generic_buffer_append_4(void *buffer, const void *data, size_t size);
ERROR_TYPE generic_buffer_append_8(void *buffer, const void *data, size_t size);
ERROR_TYPE generic_buffer_append_n(void *buffer, const void *data, size_t size, size_t element_sizeof);

ERROR_TYPE generic_buffer_push_1(void *buffer, GENERIC_ARGMENT_1 data);
ERROR_TYPE generic_buffer_push_2(void *buffer, GENERIC_ARGMENT_2 data);
ERROR_TYPE generic_buffer_push_4(void *buffer, GENERIC_ARGMENT_4 data);
ERROR_TYPE generic_buffer_push_8(void *buffer, GENERIC_ARGMENT_8 data);

#endif
