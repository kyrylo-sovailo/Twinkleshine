#ifndef OUTPUT_H
#define OUTPUT_H

#include "../commonlib/include/bool.h"
#include "../commonlib/include/macro.h"

#include <stdarg.h>

/* Initializes output module (guaranteed to succeed) */
void output_module_initialize(void);

/* Finalizes output module */
void output_module_finalize(void);

/* Opens error output (guaranteed to succeed) */
void output_open(bool error_output);

/* Closes error output (guaranteed to succeed) */
void output_close(bool error_output);

/* Writes message to error output (guaranteed to succeed) */
void output_print(bool error_output, const char *format, ...) PRINTFLIKE(2, 3);

/* Writes message to error output (guaranteed to succeed) */
void output_vprint(bool error_output, const char *format, va_list va) PRINTFLIKE(2, 0);

/* Write timestamp to error output */
void output_print_time(bool error_output);

#endif
