#ifndef STRING_H
#define STRING_H

#include "char_buffer.h"

#include <stdarg.h>
#include <stddef.h>

/* Finalizes string */
void string_finalize(struct CharBuffer *string);
/* Gets raw C string */
const char *string_get(const struct CharBuffer *string);
/* Resizes string (size does not include null terminator) */
struct Error *string_resize(struct CharBuffer *string, size_t size) NODISCARD;
/* Ensures that the string has enough capacity (capacity does not include null terminator) */
struct Error *string_reserve(struct CharBuffer *string, size_t capacity) NODISCARD;

/* Copies string */
struct Error *string_copy(struct CharBuffer *string, const struct CharBuffer *other) NODISCARD;
/* Copies string */
struct Error *string_copy_str(struct CharBuffer *string, const char *other) NODISCARD;
/* Copies string (other_size does not include null terminator) */
struct Error *string_copy_mem(struct CharBuffer *string, const char *other, size_t other_size) NODISCARD;

/* Adds character to the back of the string */
struct Error *string_push(struct CharBuffer *string, char other) NODISCARD;
/* Adds many characters to the back of the string */
struct Error *string_append(struct CharBuffer *string, const struct CharBuffer *other) NODISCARD;
/* Adds many characters to the back of the string */
struct Error *string_append_str(struct CharBuffer *string, const char *other) NODISCARD;
/* Adds many characters to the back of the string (other_size does not include null terminator) */
struct Error *string_append_mem(struct CharBuffer *string, const char *other, size_t other_size) NODISCARD;

/* Allocates memory and prints string */
struct Error *string_printf(struct CharBuffer *string, const char *format, ...) NODISCARD PRINTFLIKE(2, 3);
/* Allocates memory and prints string */
struct Error *string_vprintf(struct CharBuffer *string, const char *format, va_list va) NODISCARD PRINTFLIKE(2, 0);
/* Allocates memory and prints string starting at the end of the current string */
struct Error *string_printf_end(struct CharBuffer *string, const char *format, ...) NODISCARD PRINTFLIKE(2, 3);
/* Allocates memory and prints string starting at the end of the current string */
struct Error *string_vprintf_end(struct CharBuffer *string, const char *format, va_list va) NODISCARD PRINTFLIKE(2, 0);
/* Allocates memory and prints string starting at the end of the current string */
struct Error *string_internal_vprintf_end(struct CharBuffer *string, bool suppress_errors, const char *format, va_list va) NODISCARD PRINTFLIKE(3, 0);

/* Removes beginning at trailing spaces from string */
void string_trim(struct CharBuffer *string);
/* Removes string segment */
struct Error *string_remove(struct CharBuffer *string, size_t begin, size_t size) NODISCARD;
/* Inserts string segment */
struct Error *string_insert(struct CharBuffer *string, size_t begin, const struct CharBuffer *other) NODISCARD;
/* Inserts string segment */
struct Error *string_insert_str(struct CharBuffer *string, size_t begin, const char *other) NODISCARD;
/* Inserts string segment */
struct Error *string_insert_mem(struct CharBuffer *string, size_t begin, const char *other, size_t other_size) NODISCARD;
/* Substitutes segment */
struct Error *string_replace(struct CharBuffer *string, size_t begin, size_t size, const struct CharBuffer *other) NODISCARD;
/* Substitutes segment */
struct Error *string_replace_str(struct CharBuffer *string, size_t begin, size_t size, const char *other) NODISCARD;
/* Substitutes segment */
struct Error *string_replace_mem(struct CharBuffer *string, size_t begin, size_t size, const char *other, size_t other_size) NODISCARD;

#endif
