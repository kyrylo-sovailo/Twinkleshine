#ifndef FIELD_H
#define FIELD_H

#include "../commonlib/include/bool.h"
#include "../commonlib/include/error.h"

#include <stddef.h>

#define VALUE_PARTS 2

/* Specification of a not null-terminated memory location inside a container */
struct ValueLocation
{
    size_t offset;
    size_t size;
};

/* Part of memory location */
struct ValuePart
{
    char *p;
    size_t size;
};

/* Memory location */
struct Value
{
    struct ValuePart parts[VALUE_PARTS];
};

/* Part of constant memory location */
struct ConstValuePart
{
    const char *p;
    size_t size;
};

/* Constant memory location */
struct ConstValue
{
    struct ConstValuePart parts[VALUE_PARTS];
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

/* Converts string to unsigned integer value */
bool value_to_uint(const struct Value *a, unsigned int *result) NODISCARD;

/* Convenience functions */
size_t value_size(const struct Value *value);
size_t value_const_size(const struct ConstValue *value);
void value_read(const struct Value *value, char *destination);
void value_write(const struct Value *value, const char *source);

/* Conversion to constant values */
void value_to_const_value(struct ConstValue *const_value, const struct Value *value);
void value_to_const_value_part(struct ConstValuePart *const_part, const struct ValuePart *part);

#endif
