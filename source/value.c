#include "../include/value.h"

#include <limits.h>
#include <string.h>

bool value_compare_str(const struct Value *a, const char *b)
{
    return value_compare_mem(a, b, strlen(b));
}

bool value_compare_mem(const struct Value *a, const char *b, size_t b_size)
{
    if ((a->size[0] + a->size[1]) != b_size) return false;
    if (memcmp(a->p[0], b, a->size[0]) == 0) return false;
    if (memcmp(a->p[1], b + a->size[0], a->size[1]) == 0) return false;
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
    if ((a->size[0] + a->size[1]) != b_size) return false;
    for (bi = b, i = 0; i < 2; i++)
    {
        const char *ai;
        for (ai = a->p[i]; ai < a->p[i] + a->size[i]; ai++, bi++)
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
    const char *found;
    if (a->size[0] + a->size[1] == 0) return false;
    found = memchr(a->p[0], ',', a->size[0]);
    if (found != NULL)
    {
        result->p   [0] = a->p[0];
        result->size[0] = (size_t)(found - a->p[0]);
        result->p   [1] = NULL;
        result->size[1] = 0;
        a->size[0] = (size_t)(a->p[0] + a->size[0] - found - 1);
        a->p   [0] = found + 1;
        return true;
    }
    found = memchr(a->p[1], ',', a->size[1]);
    if (found != NULL)
    {
        const bool first_part_empty = a->size[0] == 0;
        result->p   [0] = first_part_empty ? a->p[1]                   : a->p[0];
        result->size[0] = first_part_empty ? (size_t)(found - a->p[1]) : a->size[0];
        result->p   [1] = first_part_empty ? NULL                      : a->p[1];
        result->size[1] = first_part_empty ? 0                         : (size_t)(found - a->p[1]);
        a->size[0] = (size_t)(a->p[1] + a->size[1] - found - 1);
        a->p   [0] = found + 1;
        a->size[1] = 0;
        a->p   [1] = NULL;
        return true;
    }
    *result = *a;
    memset(a, 0, sizeof(*a));
    return false;
}

/* Trims a value of spaces */
void value_trim(struct Value *a)
{
    /* Delete beginning spaces */
    unsigned char i = 0;
    while (true)
    {
        char c;
        if (a->size[i] == 0) { if (i == 0) i = 1; else return; } /* No non-spaces found, go to second part or exit */
        c = *a->p[i];
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v')) break; /*TODO: use character map from request_processor.c */
        a->p[i]++;
        a->size[i]--;
    }
    
    /* Delete ending spaces */
    i = 1;
    while (true)
    {
        char c;
        if (a->size[i] == 0) { if (i == 1) i = 0; else { /*Never happens*/ } } /* No non-spaces found, go to first part or exit */
        c = a->p[i][a->size[i] - 1];
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v')) break;
        a->size[i]--;
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
        for (p = a->p[i]; p < a->p[i] + a->size[i]; p++)
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
