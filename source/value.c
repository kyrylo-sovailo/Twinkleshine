#include "../include/value.h"

#include <limits.h>
#include <string.h>

bool value_compare_str(const char *a_buffer, const struct Value *a, const char *b)
{
    return value_compare_mem(a_buffer, a, b, strlen(b));
}

bool value_compare_mem(const char *a_buffer, const struct Value *a, const char *b, size_t b_length)
{
    return a->length == b_length && memcmp(a_buffer + a->offset, b, b_length) == 0;
}

bool value_compare_case_str(const char *a_buffer, const struct Value *a, const char *b)
{
    return value_compare_case_mem(a_buffer, a, b, strlen(b));
}

bool value_compare_case_mem(const char *a_buffer, const struct Value *a, const char *b, size_t b_length)
{
    size_t i;
    const char *ai, *bi;
    if (a->length != b_length) return false;
    for (i = 0, ai = a_buffer + a->offset, bi = b; i < b_length; i++, ai++, bi++)
    {
        char ac = *ai;
        char bc = *bi;
        if (ac >= 'A' && ac <= 'Z') ac += ('a' - 'A');
        /* if (bc >= 'A' && bc <= 'Z') bc += ('a' - 'A'); */ /* b is lowercase */
        if (ac != bc) return false;
    }
    return true;
}

bool value_parse_comma(const char *a_buffer, struct Value *a, struct Value *result)
{
    const char *found;
    if (a->length == 0) return false;
    found = memchr(a_buffer + a->offset, ',', a->length);
    if (found == NULL)
    {
        *result = *a;
        a->offset += a->length;
        a->length = 0;
    }
    else
    {
        result->offset = a->offset;
        result->length = (size_t)(found - a_buffer);
        a->offset += result->length + 1;
        a->length -= result->length + 1;
    }
    return true;
}

void value_trim(const char *buffer, struct Value *a)
{
    /* Delete beginning spaces */
    while (true)
    {
        char c;
        if (a->length == 0) return; /* The string is all spaces */
        c = buffer[a->offset];
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v')) break; /*TODO: use character map from request_processor.c */
        a->offset++;
        a->length--;
    }

    /* Delete ending spaces */
    while (true)
    {
        char c;
        c = buffer[a->offset + a->length - 1];
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v')) break;
        a->length--;
    }
}

bool value_to_uint(const char *buffer, struct Value *a, unsigned int *result)
{
    size_t offset;
    unsigned int local_result;
    local_result = 0;
    for (offset = a->offset; offset < a->offset + a->length; offset++)
    {
        char c;
        if (local_result >= (UINT_MAX - 9) / 10) return false; /* Number too big */
        c = buffer[offset];
        if (!(c >= '0' && c <= '9')) return false;
        local_result = 10 * local_result + (unsigned int)(c - '0');
    }
    if (result != NULL) *result = local_result;
    return true;
}
