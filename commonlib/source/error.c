#include "../include/error.h"
#include "../include/path.h"
#include "../include/string.h"
#include "../../include/memory.h"
#include "../../include/output.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct Error
{
    struct CharBuffer message;
    struct Error *next;
};

struct Error *error_internal_allocate(const char *format, ...)
{
    /* Create new error buffer */
    struct Error *error = (struct Error*)count_malloc(sizeof(*error));
    if (error != NULL)
    {
        /* Print */
        const struct Error zero = ZERO_INIT;
        va_list va;
        *error = zero;
        va_start(va, format);
        if (string_internal_vprint_append(&error->message, true, format, va)) {}
        va_end(va);
        return error;
    }
    else
    {
        /* Out of memory */
        return PANIC;
    }
}

struct Error *error_internal_allocate_append(struct Error *error, const char *format, ...)
{
    /* Create new error buffer */
    struct Error *new_error = (struct Error*)count_malloc(sizeof(*new_error));
    if (new_error != NULL)
    {
        /* Print */
        const struct Error zero = ZERO_INIT;
        va_list va;
        *error = zero;
        va_start(va, format);
        if (string_internal_vprint_append(&new_error->message, true, format, va)) {}
        va_end(va);

        /* Append */
        new_error->next = error;
        return new_error;
    }
    else
    {
        /* Out of memory */
        struct Error *error_i;
        if (error == NULL) return PANIC; /* Shouldn't normally happen. Should I "if (!error) return error;" at the beginning? */
        if (error == PANIC) return PANIC; /* Not much can be done */
        
        /* Go to the last allocated error and set it to PANIC */
        error_i = error;
        while (true)
        {
            if (error_i->next == NULL || error_i->next == PANIC) break;
            error_i = error_i->next;
        }
        error_i->next = PANIC;
        return error;
    }
}

int error_get_exit_code(const struct Error *error)
{
    const int invalid_line = 1000 * 1000;
    const struct Error *error_i;
    const char *p_end;
    if (error == OK) return invalid_line + 1;
    if (error == PANIC) return invalid_line + 2;

    /* Find last error */
    error_i = error;
    while (true)
    {
        if (error_i->next == OK) break;
        error_i = error_i->next;
        if (error_i == PANIC) break;
    }

    /* Find line number */
    if (error->message.p == NULL) return invalid_line + 3;
    p_end = error->message.p + error->message.size;
    while (true)
    {
        const char *p_number_begin;
        char *p_number_end;
        unsigned long line;
        while (p_end - 1 > error->message.p && *(p_end - 1) != ':') p_end--; /* Skip non : */
        if (*p_end != ':') return invalid_line + 4; /* No : found */
        p_number_begin = p_end - 1;
        while (p_number_begin - 1 > error->message.p && *(p_number_begin - 1) >= '0' && *(p_number_begin - 1) <= '9') p_number_begin--; /* Skip number */
        if (!(*p_number_begin >= '0' && *p_number_begin <= '9')) { p_end = p_number_begin - 1; continue; } /* No number found */
        line = strtoul(p_number_begin, &p_number_end, 10);
        if (p_number_end != p_end) { p_end = p_number_begin - 1; continue; } /* Number invalid */
        return (int)line;
    }
    return invalid_line + 5;
}

void error_print(const struct Error *error)
{
    /* Print program name */
    output_open();
    output_print("%s:\n", (g_application.p != NULL) ? g_application.p : "APPLICATION NULL");

    /* Print last error */
    if (error == OK)
    {
        output_print("ERROR OK\n");
    }
    else if (error == PANIC)
    {
        output_print("ERROR PANIC\n");
    }
    else
    {
        const struct Error *error_i;
        unsigned int error_number;

        /* Print last error */
        error_i = error;
        while (true)
        {
            if (error_i->next == OK) break;
            error_i = error_i->next;
            if (error_i == PANIC) break;
        }
        output_print("%s\n", (error_i == PANIC) ? "ERROR PANIC" : ((error_i->message.p != NULL) ? error_i->message.p : "ERROR NULL"));

        /* Print traceback */
        output_print("Traceback (most recent call last):\n");
        error_number = 0;
        error_i = error;
        while (true)
        {
            error_number++;
            if (error_i == OK) break;
            output_print("%d. %s\n", error_number, (error_i == PANIC) ? "ERROR PANIC" : ((error_i->message.p != NULL) ? error_i->message.p : "ERROR NULL"));
            if (error_i == PANIC) break;
            error_i = error_i->next;
        }

        /* Print time */
        output_print_time();
    }

    /* Print trailing newline */
    output_print("\n");
    output_close();
}

void error_finalize(struct Error *error)
{
    while (error != NULL && error != PANIC)
    {
        struct Error *next_error;
        string_finalize(&error->message);
        next_error = error->next;
        count_free(error, sizeof(*error));
        error = next_error;
    }
}
