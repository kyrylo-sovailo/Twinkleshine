#ifndef COMMONLIB_STRING_H
#define COMMONLIB_STRING_H

#include "char_buffer.h"
#include "error.h"
#include "macro.h"

#include <stdarg.h>
#include <stddef.h>

/*
String is a CharBuffer that keeps a null terminator at the end.
CharBuffer fields are implemented in the following way:
 - size     - String size (does not include null terminator, unlike other buffers)
 - capacity - Buffer capacity (includes null terminator, just like other buffers)
*/

/* Initializes string */
void string_initialize(struct CharBuffer *string);

/* Finalizes string */
void string_finalize(struct CharBuffer *string);

/* Gets raw C string */
const char *string_get(const struct CharBuffer *string);

/* Sets the size to zero */
void string_zero(struct CharBuffer *string);

/* Resizes string (size does not include null terminator) */
ERROR_TYPE string_resize(struct CharBuffer *string, size_t size) NODISCARD;

/* Ensures that the string has enough capacity (capacity does not include null terminator) */
ERROR_TYPE string_reserve(struct CharBuffer *string, size_t capacity) NODISCARD;

/* Copies string */
ERROR_TYPE string_copy(struct CharBuffer *string, const struct CharBuffer *other) NODISCARD;
ERROR_TYPE string_copy_str(struct CharBuffer *string, const char *other) NODISCARD;
ERROR_TYPE string_copy_mem(struct CharBuffer *string, const char *other, size_t other_size) NODISCARD;

/* Adds character to the back of the string */
ERROR_TYPE string_push(struct CharBuffer *string, char other) NODISCARD;

/* Adds many characters to the back of the string */
ERROR_TYPE string_append(struct CharBuffer *string, const struct CharBuffer *other) NODISCARD;
ERROR_TYPE string_append_str(struct CharBuffer *string, const char *other) NODISCARD;
ERROR_TYPE string_append_mem(struct CharBuffer *string, const char *other, size_t other_size) NODISCARD;

/* Allocates memory and prints string */
ERROR_TYPE string_print(struct CharBuffer *string, const char *format, ...) NODISCARD PRINTFLIKE(2, 3);
ERROR_TYPE string_vprint(struct CharBuffer *string, const char *format, va_list va) NODISCARD PRINTFLIKE(2, 0);
ERROR_TYPE string_print_append(struct CharBuffer *string, const char *format, ...) NODISCARD PRINTFLIKE(2, 3);
ERROR_TYPE string_vprint_append(struct CharBuffer *string, const char *format, va_list va) NODISCARD PRINTFLIKE(2, 0);
#ifdef ERROR_TRACE
ERROR_TYPE string_internal_vprint_append(struct CharBuffer *string, bool suppress_errors, const char *format, va_list va) NODISCARD PRINTFLIKE(3, 0);
#else
ERROR_TYPE string_internal_vprint_append(struct CharBuffer *string, const char *format, va_list va) NODISCARD PRINTFLIKE(2, 0);
#endif

/* Removes beginning at trailing spaces from string */
void string_trim(struct CharBuffer *string);

/* Removes string segment */
ERROR_TYPE string_remove(struct CharBuffer *string, size_t begin, size_t size) NODISCARD;

/* Inserts string segment */
ERROR_TYPE string_insert(struct CharBuffer *string, size_t begin, const struct CharBuffer *other) NODISCARD;
ERROR_TYPE string_insert_str(struct CharBuffer *string, size_t begin, const char *other) NODISCARD;
ERROR_TYPE string_insert_mem(struct CharBuffer *string, size_t begin, const char *other, size_t other_size) NODISCARD;

/* Substitutes segment */
ERROR_TYPE string_replace(struct CharBuffer *string, size_t begin, size_t size, const struct CharBuffer *other) NODISCARD;
ERROR_TYPE string_replace_str(struct CharBuffer *string, size_t begin, size_t size, const char *other) NODISCARD;
ERROR_TYPE string_replace_mem(struct CharBuffer *string, size_t begin, size_t size, const char *other, size_t other_size) NODISCARD;

#ifdef WIN32

/* Wide strings */
ERROR_TYPE string_to_wstring(struct WCharBuffer *wstring, const struct CharBuffer *string) NODISCARD;
ERROR_TYPE string_to_string(struct CharBuffer *string, const struct WCharBuffer *wstring) NODISCARD;

#endif

#endif
