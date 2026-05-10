#include "../include/extended_error.h"
#include "../commonlib/include/error.h"

struct ExError exerror_internal_allocate(enum ExErrorFlag flags, const cchar_t *format, ...)
{
    struct ExError result;
    va_list va;
    va_start(va, format);
    result.error = error_internal_vallocate(format, va);
    result.flags = flags;
    va_end(va);
    return result;
}

struct ExError exerror_internal_allocate_append(struct ExError error, const cchar_t *format, ...)
{
    struct ExError result;
    va_list va;
    va_start(va, format);
    result.error = error_internal_vallocate_append(error.error, format, va);
    result.flags = error.flags;
    va_end(va);
    return result;
}

struct ExError exerror_internal_allocate_append_extend(struct Error *error, enum ExErrorFlag flags, const cchar_t *format, ...)
{
    struct ExError result;
    va_list va;
    va_start(va, format);
    result.error = error_internal_vallocate_append(error, format, va);
    result.flags = flags;
    va_end(va);
    return result;
}
