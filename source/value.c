#include "../include/value.h"

#include <limits.h>
#include <string.h>

bool value_compare_str(const struct Value *a, const char *b)
{
    return value_compare_mem(a, b, strlen(b));
}

bool value_compare_mem(const struct Value *a, const char *b, size_t b_size)
{
    if ((a->parts[0].size + a->parts[1].size) != b_size) return false;
    if (memcmp(a->parts[0].p, b,                    a->parts[0].size) != 0) return false;
    if (memcmp(a->parts[1].p, b + a->parts[0].size, a->parts[1].size) != 0) return false;
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
    if ((a->parts[0].size + a->parts[1].size) != b_size) return false;
    for (bi = b, i = 0; i < 2; i++)
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
    bool success = false;
    unsigned char i;
    memset(&result[0], 0, 2 * sizeof(struct ValuePart));
    for (i = 0; i < 2; i++)
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
            memcpy(&result->parts[i], &a->parts[i], sizeof(struct ValuePart));
            memset(&a->parts[i], 0, sizeof(struct ValuePart));
        }
    }
    return success;
}

/* Trims a value of spaces */
void value_trim(struct Value *a)
{
    /* Delete beginning spaces */
    unsigned char i = 0;
    while (true)
    {
        char c;
        if (a->parts[i].size == 0) { if (i == 0) i = 1; else return; } /* No non-spaces found, go to second part or exit */
        c = *a->parts[i].p;
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v')) break; /*TODO: use character map from request_processor.c */
        a->parts[i].p++;
        a->parts[i].size--;
    }
    
    /* Delete ending spaces */
    i = 1;
    while (true)
    {
        char c;
        if (a->parts[i].size == 0) { if (i == 1) i = 0; else { /*Never happens*/ } } /* No non-spaces found, go to first part or exit */
        c = a->parts[i].p[a->parts[i].size - 1];
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v')) break;
        a->parts[i].size--;
    }
}

/* Converts  */
bool value_to_uint(const struct Value *a, unsigned int *result)
{
    unsigned int local_result = 0;
    unsigned char i;
    for (i = 0; i < 2; i++)
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
