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
const cchar_t *string_get(const struct CharBuffer *string);

/* Gets whether string is ASCII */
bool string_is_ascii(const struct CharBuffer *string);
bool string_is_ascii_str(const cchar_t *string);
bool string_is_ascii_mem(const cchar_t *string, size_t size);

/* Sets the size to zero */
void string_zero(struct CharBuffer *string);

/* Resizes string (size does not include null terminator) */
ERROR_TYPE string_resize(struct CharBuffer *string, size_t size) NODISCARD;

/* Ensures that the string has enough capacity (capacity does not include null terminator) */
ERROR_TYPE string_reserve(struct CharBuffer *string, size_t capacity) NODISCARD;

/* Copies string */
ERROR_TYPE string_copy(struct CharBuffer *string, const struct CharBuffer *other) NODISCARD;
ERROR_TYPE string_copy_str(struct CharBuffer *string, const cchar_t *other) NODISCARD;
ERROR_TYPE string_copy_mem(struct CharBuffer *string, const cchar_t *other, size_t other_size) NODISCARD;

/* Adds character to the back of the string */
ERROR_TYPE string_push(struct CharBuffer *string, cchar_t other) NODISCARD;

/* Adds many characters to the back of the string */
ERROR_TYPE string_append(struct CharBuffer *string, const struct CharBuffer *other) NODISCARD;
ERROR_TYPE string_append_str(struct CharBuffer *string, const cchar_t *other) NODISCARD;
ERROR_TYPE string_append_mem(struct CharBuffer *string, const cchar_t *other, size_t other_size) NODISCARD;

/* Allocates memory and prints string */
ERROR_TYPE string_print(struct CharBuffer *string, const cchar_t *format, ...) NODISCARD PRINTFLIKE(2, 3);
ERROR_TYPE string_vprint(struct CharBuffer *string, const cchar_t *format, va_list va) NODISCARD PRINTFLIKE(2, 0);
ERROR_TYPE string_print_append(struct CharBuffer *string, const cchar_t *format, ...) NODISCARD PRINTFLIKE(2, 3);
ERROR_TYPE string_vprint_append(struct CharBuffer *string, const cchar_t *format, va_list va) NODISCARD PRINTFLIKE(2, 0);
ERROR_TYPE string_internal_vprint_append(struct CharBuffer *string, bool suppress_errors, const cchar_t *format, va_list va) NODISCARD PRINTFLIKE(3, 0);

/* Removes beginning at trailing spaces from string */
void string_trim(struct CharBuffer *string);

/* Removes string segment */
ERROR_TYPE string_remove(struct CharBuffer *string, size_t begin, size_t size) NODISCARD;

/* Inserts string segment */
ERROR_TYPE string_insert(struct CharBuffer *string, size_t begin, const struct CharBuffer *other) NODISCARD;
ERROR_TYPE string_insert_str(struct CharBuffer *string, size_t begin, const cchar_t *other) NODISCARD;
ERROR_TYPE string_insert_mem(struct CharBuffer *string, size_t begin, const cchar_t *other, size_t other_size) NODISCARD;

/* Substitutes segment */
ERROR_TYPE string_replace(struct CharBuffer *string, size_t begin, size_t size, const struct CharBuffer *other) NODISCARD;
ERROR_TYPE string_replace_str(struct CharBuffer *string, size_t begin, size_t size, const cchar_t *other) NODISCARD;
ERROR_TYPE string_replace_mem(struct CharBuffer *string, size_t begin, size_t size, const cchar_t *other, size_t other_size) NODISCARD;

ERROR_TYPE string_to_wstring(const nchar_t *np, size_t nsize, wchar_t *wp, size_t *wsize) NODISCARD;
ERROR_TYPE string_to_nstring(const wchar_t *wp, size_t wsize, nchar_t *np, size_t *nsize) NODISCARD;
ERROR_TYPE string_internal_to_wstring(const nchar_t *np, size_t nsize, wchar_t *wp, size_t *wsize, bool suppress_errors) NODISCARD;
ERROR_TYPE string_internal_to_nstring(const wchar_t *wp, size_t wsize, nchar_t *np, size_t *nsize, bool suppress_errors) NODISCARD;

#endif
