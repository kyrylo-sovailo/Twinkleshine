#include "../include/error.h"
#include "../../include/output.h"
#include "../include/string.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

struct Error
{
    struct CharBuffer message;
    struct Error *next;
};

const cchar_t *g_application = NULL;

struct Error *error_internal_allocate(const cchar_t *format, ...)
{
    struct Error *result;
    va_list va;
    va_start(va, format);
    result = error_internal_vallocate(format, va);
    va_end(va);
    return result;
}

struct Error *error_internal_vallocate(const cchar_t *format, va_list va)
{
    /* Create new error buffer */
    struct Error *error = (struct Error*)malloc(sizeof(*error));
    if (error != NULL)
    {
        /* Print */
        const struct Error zero = ZERO_INIT;
        *error = zero;
        PIGNORE(string_internal_vprint_append(&error->message, true, format, va));
        return error;
    }
    else
    {
        /* Out of memory */
        return PANIC;
    }
}

struct Error *error_internal_allocate_append(struct Error *error, const cchar_t *format, ...)
{
    struct Error *result;
    va_list va;
    va_start(va, format);
    result = error_internal_vallocate_append(error, format, va);
    va_end(va);
    return result;
}

struct Error *error_internal_vallocate_append(struct Error *error, const cchar_t *format, va_list va)
{
    /* Create new error buffer */
    struct Error *new_error = (struct Error*)malloc(sizeof(*new_error));
    if (new_error != NULL)
    {
        /* Print */
        const struct Error zero = ZERO_INIT;
        *new_error = zero;
        PIGNORE(string_internal_vprint_append(&new_error->message, true, format, va));

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
        
        /* Go to the first allocated error and set it to PANIC for it to print correctly */
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
    const cchar_t *p_end;
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
        const cchar_t *p_number_begin;
        cchar_t *p_number_end;
        unsigned long line;
        while (p_end - 1 > error->message.p && *(p_end - 1) != ':') p_end--; /* Skip non : */
        if (*p_end != ':') return invalid_line + 4; /* No : found */
        p_number_begin = p_end - 1;
        while (p_number_begin - 1 > error->message.p && *(p_number_begin - 1) >= '0' && *(p_number_begin - 1) <= '9') p_number_begin--; /* Skip number */
        if (!(*p_number_begin >= '0' && *p_number_begin <= '9')) { p_end = p_number_begin - 1; continue; } /* No number found */
        line = COMMON_WCS(toul(p_number_begin, &p_number_end, 10));
        if (p_number_end != p_end) { p_end = p_number_begin - 1; continue; } /* Number invalid */
        return (int)line;
    }
    return invalid_line + 5;
}

void error_print(const struct Error *error, const struct Client *client)
{
    const cchar_t *message;

    /* Print program name */
    output_open(true);
    if (g_application == NULL) message = "APPLICATION NULL";
    else message = g_application;
    output_print(true, COMMON_S COMMON_L(":") COMMON_N, message);
    if (client != NULL) output_print_client(true, client);

    /* Print error */
    if (error == OK)
    {
        output_print(true, COMMON_L("ERROR OK") COMMON_N);
    }
    else if (error == PANIC)
    {
        output_print(true, COMMON_L("ERROR PANIC") COMMON_N);
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
        if (error == PANIC) message = COMMON_L("ERROR PANIC");
        else if (error_i->message.p == NULL) message = COMMON_L("ERROR NULL");
        else message = error_i->message.p;
        output_print(true, COMMON_S COMMON_N, message);

        /* Print traceback */
        output_print(true, COMMON_L("Traceback (most recent call last):") COMMON_N);
        error_number = 0;
        error_i = error;
        while (true)
        {
            error_number++;
            if (error_i == OK) break;
            if (error == PANIC) message = COMMON_L("ERROR PANIC");
            else if (error_i->message.p == NULL) message = COMMON_L("ERROR NULL");
            else message = error_i->message.p;
            output_print(true, COMMON_L("%d. ") COMMON_S COMMON_N, error_number, message);
            if (error_i == PANIC) break;
            error_i = error_i->next;
        }
    }

    /* Print time */
    output_print_time(true);
    output_print(true, COMMON_N);
    output_close(true);
}

void error_finalize(struct Error *error)
{
    while (error != NULL && error != PANIC)
    {
        struct Error *next_error;
        string_finalize(&error->message);
        next_error = error->next;
        free(error);
        error = next_error;
    }
}
