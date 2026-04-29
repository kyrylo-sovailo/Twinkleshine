#ifndef COMMONLIB_PATH_H
#define COMMONLIB_PATH_H

#include "bool.h"
#include "char_buffer.h"
#include "error.h"

#include <stdarg.h>
#include <stddef.h>

/*
Path is a String
It refers to an absolute or relative path, file or directory
Path maintains the correct form at all times:
no double slashes, no trailing slashes (except for root), no slashes on backslash-based systems, no ., no reducible ..
empty path is a valid relative path, equivalent to .
*/

/* Application name, used by error module */
extern struct CharBuffer g_application;
extern struct CharBuffer g_directory;
extern struct CharBuffer g_working_directory;

/* Initializes/finalizes global variables */
ERROR_TYPE path_module_initialize(int argc, cchar_t **argv) NODISCARD;
void path_module_finalize(void);

/* Copies file name to path, while also caring about slashes and parent directories */
ERROR_TYPE path_copy(struct CharBuffer *path, const struct CharBuffer *other) NODISCARD;
ERROR_TYPE path_copy_str(struct CharBuffer *path, const cchar_t *other) NODISCARD;
ERROR_TYPE path_copy_mem(struct CharBuffer *path, const cchar_t *other, size_t other_size) NODISCARD;

/* Appends file name to path, while also caring about slashes and parent directories */
ERROR_TYPE path_append(struct CharBuffer *path, const struct CharBuffer *other) NODISCARD;
ERROR_TYPE path_append_str(struct CharBuffer *path, const cchar_t *other) NODISCARD;
ERROR_TYPE path_append_mem(struct CharBuffer *path, const cchar_t *other, size_t other_size) NODISCARD;

/* Allocates memory and prints path, while also caring about slashes and parent directories */
ERROR_TYPE path_print(struct CharBuffer *path, const cchar_t *format, ...) NODISCARD PRINTFLIKE(2, 3);
ERROR_TYPE path_vprint(struct CharBuffer *path, const cchar_t *format, va_list va) NODISCARD PRINTFLIKE(2, 0);
ERROR_TYPE path_print_append(struct CharBuffer *path, const cchar_t *format, ...) NODISCARD PRINTFLIKE(2, 3);
ERROR_TYPE path_vprint_append(struct CharBuffer *path, const cchar_t *format, va_list va) NODISCARD PRINTFLIKE(2, 0);

/* Returns whether the path is absolute */
bool path_absolute(const struct CharBuffer *path);
bool path_absolute_str(const cchar_t *path);
bool path_absolute_mem(const cchar_t *path, size_t path_size);

/* Gets executable path */
ERROR_TYPE path_get_executable_path(struct CharBuffer *path, int argc, cchar_t **argv) NODISCARD;
/* Gets current working directory */
ERROR_TYPE path_get_working_directory(struct CharBuffer *path) NODISCARD;
/* Gets directory of the path (directory == path is allowed) */
ERROR_TYPE path_get_directory(struct CharBuffer *directory, const struct CharBuffer *path, bool append_dotdot_if_dotdot) NODISCARD;
/* Gets basename of the path (basename == path is allowed) */
ERROR_TYPE path_get_basename(struct CharBuffer *basename, const struct CharBuffer *path) NODISCARD;

#endif
