#ifndef FIELD_H
#define FIELD_H

#include "bool.h"
#include "error.h"

#include <stddef.h>

/* Not null-terminated string */
struct Value
{
    size_t offset;
    size_t length;
};

/* Compares two values */
bool value_compare_str(const char *a_buffer, const struct Value *a, const char *b);
bool value_compare_mem(const char *a_buffer, const struct Value *a, const char *b, size_t b_length);

/* Compares two values, case insensitive, b is lowercase */
bool value_compare_case_str(const char *a_buffer, const struct Value *a, const char *b);
bool value_compare_case_mem(const char *a_buffer, const struct Value *a, const char *b, size_t b_length);

/* Extracts a word from comma-separated value in a */
bool value_parse_comma(const char *a_buffer, struct Value *a, struct Value *result) NODISCARD;

/* Trims a value of spaces */
void value_trim(const char *buffer, struct Value *a);

/* Converts  */
bool value_to_uint(const char *buffer, struct Value *a, unsigned int *result) NODISCARD;

#endif
