#ifndef FIELD_H
#define FIELD_H

#include "bool.h"
#include "error.h"

#include <stddef.h>

/* Specification of a not null-terminated memory location */
struct ValueRange
{
    size_t offset;
    size_t size;
};

/* Not null-terminated value, may be split between two buffers */
struct Value
{
    const char *p[2];
    size_t size[2];
};

/* Compares two values */
bool value_compare_str(const struct Value *a, const char *b);
bool value_compare_mem(const struct Value *a, const char *b, size_t b_size);

/* Compares two values, case insensitive, b is lowercase */
bool value_compare_case_str(const struct Value *a, const char *b);
bool value_compare_case_mem(const struct Value *a, const char *b, size_t b_size);

/* Extracts a word from comma-separated value in a */
bool value_parse_comma(struct Value *a, struct Value *result) NODISCARD;

/* Trims a value of spaces */
void value_trim(struct Value *a);

/* Converts  */
bool value_to_uint(const struct Value *a, unsigned int *result) NODISCARD;

#endif
