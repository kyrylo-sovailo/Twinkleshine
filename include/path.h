#ifndef PATH_H
#define PATH_H

#include "bool.h"
#include "error.h"
#include "string.h"

extern struct CharBuffer g_application;

/* Set path to application's path */
struct Error *path_set_application(struct CharBuffer *application, const char *argv0) NODISCARD;

/* Set path to parent directory */
struct Error *path_up(struct CharBuffer *path, size_t count) NODISCARD;
/* Append file name to path */
struct Error *path_append(struct CharBuffer *path, const struct CharBuffer *other, bool is_directory) NODISCARD;
/* Append file name to path */
struct Error *path_append_str(struct CharBuffer *path, const char *other, bool is_directory) NODISCARD;

/* Allocates memory and prints path */
struct Error *path_printf(struct CharBuffer *path, const char *format, ...) NODISCARD PRINTFLIKE(2, 3);
/* Allocates memory and prints path */
struct Error *path_vprintf(struct CharBuffer *path, const char *format, va_list va) NODISCARD PRINTFLIKE(2, 0);
/* Allocates memory and prints path starting at the end of the current path */
struct Error *path_printf_end(struct CharBuffer *path, const char *format, ...) NODISCARD PRINTFLIKE(2, 3);
/* Allocates memory and prints path starting at the end of the current path */
struct Error *path_vprintf_end(struct CharBuffer *path, const char *format, va_list va) NODISCARD PRINTFLIKE(2, 0);

#endif
