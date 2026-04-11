#ifndef COMMONLIB_BUFFER_H
#define COMMONLIB_BUFFER_H

#include "error.h"

#include <stddef.h>

#ifndef GENERIC_ARGMENT_1
    #define GENERIC_ARGMENT_1 char
    #define GENERIC_ARGMENT_2 short
    #define GENERIC_ARGMENT_4 signed
    #define GENERIC_ARGMENT_8 long
#endif

#define DECLARE_BUFFER(TYPE, STRUCT_NAME) \
struct STRUCT_NAME \
{ \
    TYPE *p; \
    size_t size; \
    size_t capacity; \
};

#define DECLARE_BUFFER_INITIALIZE(TYPE, STRUCT_NAME, FUNCTION_NAME) \
void FUNCTION_NAME ## initialize(struct STRUCT_NAME *buffer);
#define DECLARE_BUFFER_FINALIZE(TYPE, STRUCT_NAME, FUNCTION_NAME) \
void FUNCTION_NAME ## finalize(struct STRUCT_NAME *buffer);
#define DECLARE_BUFFER_RESIZE(TYPE, STRUCT_NAME, FUNCTION_NAME) \
ERROR_TYPE FUNCTION_NAME ## resize(struct STRUCT_NAME *buffer, size_t size) NODISCARD;
#define DECLARE_BUFFER_RESERVE(TYPE, STRUCT_NAME, FUNCTION_NAME) \
ERROR_TYPE FUNCTION_NAME ## reserve(struct STRUCT_NAME *buffer, size_t capacity) NODISCARD;
#define DECLARE_BUFFER_APPEND(TYPE, STRUCT_NAME, FUNCTION_NAME) \
ERROR_TYPE FUNCTION_NAME ## append(struct STRUCT_NAME *buffer, const TYPE *data, size_t size) NODISCARD;
#define DECLARE_BUFFER_PUSH(TYPE, STRUCT_NAME, FUNCTION_NAME) \
ERROR_TYPE FUNCTION_NAME ## push(struct STRUCT_NAME *buffer, TYPE data) NODISCARD;

#endif
