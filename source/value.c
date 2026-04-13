#include "../include/value.h"

#include <limits.h>
#include <string.h>

bool value_compare_str(const struct Value *a, const char *b)
{
    return value_compare_mem(a, b, strlen(b));
}

bool value_compare_mem(const struct Value *a, const char *b, size_t b_size)
{
    unsigned char i;
    if (value_size(a) != b_size) return false;
    for (i = 0; i < VALUE_PARTS; i++)
    {
        if (memcmp(a->parts[i].p, b, a->parts[i].size) != 0) return false;
        b += a->parts[i].size;
    }
    return true;
}

bool value_compare_case_str(const struct Value *a, const char *b)
{
    return value_compare_case_mem(a, b, strlen(b));
}

bool value_compare_case_mem(const struct Value *a, const char *b, size_t b_size)
{
    const char *bi;
    unsigned char i;
    if (value_size(a) != b_size) return false;
    for (bi = b, i = 0; i < VALUE_PARTS; i++)
    {
        const char *ai;
        for (ai = a->parts[i].p; ai < a->parts[i].p + a->parts[i].size; ai++, bi++)
        {
            char ac = *ai;
            char bc = *bi;
            if (ac >= 'A' && ac <= 'Z') ac += ('a' - 'A');
            /* if (bc >= 'A' && bc <= 'Z') bc += ('a' - 'A'); */ /* b is lowercase */
            if (ac != bc) return false;
        }
    }
    return true;
}

bool value_parse_comma(struct Value *a, struct Value *result)
{
    /* This might be the most idiotic piece of code I ever wrote, but it runs in a loop */
    const struct Value zero = ZERO_INIT;
    bool success = false;
    unsigned char i;
    *result = zero;
    for (i = 0; i < VALUE_PARTS; i++)
    {
        char *found;
        success |= (a->parts[i].size > 0);
        found = memchr(a->parts[i].p, ',', a->parts[i].size);
        if (found != NULL)
        {
            /* Comma found */
            result->parts[i].size = (size_t)(found - a->parts[i].p);
            result->parts[i].p = a->parts[i].p;
            a->parts[i].size = (size_t)(a->parts[i].p + a->parts[i].size - found - 1);
            a->parts[i].p = found + 1;
            return true;
        }
        else
        {
            /* Comma not found */
            result->parts[i] = a->parts[i];
            a->parts[i] = zero.parts[i];
        }
    }
    return success;
}

void value_trim(struct Value *a)
{
    /* Delete beginning spaces */
    unsigned char i;
    for (i = 0;;)
    {
        char c;
        while (a->parts[i].size == 0) { i++; if (i == VALUE_PARTS) return; } /* No non-spaces found, go to second part or exit */
        c = *a->parts[i].p;
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v')) break; /*TODO: use character map from request_processor.c */
        a->parts[i].p++;
        a->parts[i].size--;
    }
    
    /* Delete ending spaces */
    for (i = VALUE_PARTS - 1;;)
    {
        char c;
        while (a->parts[i].size == 0) i--; /* No non-spaces found, go to first part or exit (no boundary check because it must find non-space) */
        c = a->parts[i].p[a->parts[i].size - 1];
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v')) break;
        a->parts[i].size--;
    }
}

bool value_to_uint(const struct Value *a, unsigned int *result)
{
    unsigned int local_result = 0;
    unsigned char i;
    for (i = 0; i < VALUE_PARTS; i++)
    {
        const char *p;
        for (p = a->parts[i].p; p < a->parts[i].p + a->parts[i].size; p++)
        {
            char c;
            if (local_result >= (UINT_MAX - 9) / 10) return false; /* Number too big */
            c = *p;
            if (!(c >= '0' && c <= '9')) return false;
            local_result = 10 * local_result + (unsigned int)(c - '0');
        }
    }
    *result = local_result;
    return true;
}

void value_to_const_value(struct ConstValue *const_value, const struct Value *value)
{
    unsigned char i;
    for (i = 0; i < VALUE_PARTS; i++)
    {
        const_value->parts[i].p = value->parts[i].p;
        const_value->parts[i].size = value->parts[i].size;
    }
}

void value_to_const_value_part(struct ConstValuePart *const_part, const struct ValuePart *part)
{
    const_part->p = part->p;
    const_part->size = part->size;
}

size_t value_size(const struct Value *value)
{
    size_t sum = 0;
    unsigned char i;
    for (i = 0; i < VALUE_PARTS; i++) sum += value->parts[i].size;
    return sum;
}

size_t value_const_size(const struct ConstValue *value)
{
    size_t sum = 0;
    unsigned char i;
    for (i = 0; i < VALUE_PARTS; i++) sum += value->parts[i].size;
    return sum;
}

void value_read(const struct Value *value, char *destination)
{
    unsigned char i;
    for (i = 0; i < VALUE_PARTS; i++)
    {
        memcpy(destination, value->parts[i].p, value->parts[i].size);
        destination += value->parts[i].size;
    }
}

void value_write(const struct Value *value, const char *source)
{
    unsigned char i;
    for (i = 0; i < VALUE_PARTS; i++)
    {
        memcpy(value->parts[i].p, source, value->parts[i].size);
        source += value->parts[i].size;
    }
}
