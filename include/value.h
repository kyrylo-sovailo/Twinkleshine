#ifndef FIELD_H
#define FIELD_H

#include "../commonlib/include/bool.h"
#include "../commonlib/include/macro.h"

#include <stddef.h>

#define VALUE_PARTS 2

/* Specification of a not null-terminated memory location inside a container */
struct ValueLocation
{
    size_t offset;
    size_t size;
};

/* Constant continuous memory */
struct ConstantContinuousValue
{
    const char *p;
    size_t size;
};

/* Mutable continuous memory */
struct MutableContinuousValue
{
    char *p;
    size_t size;
};

/* Constant non-continuous memory */
struct ConstantValue
{
    struct ConstantContinuousValue parts[VALUE_PARTS];
};

/* Mutable non-continuous memory */
struct MutableValue
{
    struct MutableContinuousValue parts[VALUE_PARTS];
};

/* Maybe mutable continuous memory */
union ContinuousValue
{
    struct ConstantContinuousValue c;
    struct MutableContinuousValue m;
};

/* Maybe mutable non-continuous memory */
union Value
{
    struct ConstantValue c;
    struct MutableValue m;
};

/* Compares two values */
bool value_compare_str(const struct ConstantValue *a, const char *b);
bool value_compare_mem(const struct ConstantValue *a, const char *b, size_t b_size);

/* Compares two values, case insensitive, b is lowercase */
bool value_compare_case_str(const struct ConstantValue *a, const char *b);
bool value_compare_case_mem(const struct ConstantValue *a, const char *b, size_t b_size);

/* Extracts a word from comma-separated value in a */
bool value_parse_comma(struct ConstantValue *a, struct ConstantValue *result) NODISCARD;

/* Trims a value of spaces */
void value_trim(struct ConstantValue *a);

/* Converts string to unsigned integer value */
bool value_to_uint(const struct ConstantValue *a, unsigned int *result) NODISCARD;

/* Convenience functions */
size_t value_size(const struct ConstantValue *value);
void value_first(struct ConstantValue *value, size_t size);
void value_second(struct ConstantValue *value, size_t size);
void value_first_second(struct ConstantValue *value, struct ConstantValue *first, size_t size);
void value_read(const struct ConstantValue *value, char *destination);
void value_write(const struct MutableValue *value, const char *source);
void value_to_continuous(const struct ConstantValue *value, struct ConstantContinuousValue *continuous_value, char *buffer);

#endif
