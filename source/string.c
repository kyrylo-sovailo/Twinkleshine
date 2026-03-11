#include "../include/string.h"
#include "../include/buffer_implementation.h"

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef unsigned char Flag;
enum
{
    FLAG_HASH           = 0x01,
    FLAG_ZERO           = 0x02,
    FLAG_MINUS          = 0x04,
    FLAG_PLUS           = 0x08,
    FLAG_SPACE          = 0x10
};

typedef unsigned char Length;
enum
{
    LENGTH_NONE,
    LENGTH_SHORT_SHORT, /* hh */
    LENGTH_SHORT,       /* h  */
    LENGTH_LONG,        /* l  */
    LENGTH_LONG_LONG,   /* ll */
    LENGTH_MAX,         /* j  */
    LENGTH_SIZE,        /* z  */
    LENGTH_PTRDIFF,     /* t  */
    LENGTH_LONG_DOUBLE  /* L  */
};

void string_finalize(struct CharBuffer *string)
{
    generic_buffer_finalize(string);
}

const char *string_get(const struct CharBuffer *string)
{
    return (string->p == NULL) ? "" : string->p;
}

struct Error *string_resize(struct CharBuffer *string, size_t size)
{
    if (size + 1 > string->capacity)
    {
        char *new_p;
        size_t new_capacity = (string->capacity < 2) ? 2 : string->capacity;
        while (size + 1 > new_capacity) new_capacity *= 2;
        new_p = realloc(string->p, new_capacity * sizeof(*string->p));
        ARET(new_p != NULL);
        string->capacity = new_capacity;
        string->p = new_p;
    }
    string->size = size;
    string->p[size] = '\0';
    return OK;
}

struct Error *string_reserve(struct CharBuffer *string, size_t capacity)
{
    if (capacity + 1 > string->capacity)
    {
        char *new_p;
        size_t new_capacity = (string->capacity < 2) ? 2 : string->capacity;
        while (capacity + 1 > new_capacity) new_capacity *= 2;
        new_p = realloc(string->p, new_capacity * sizeof(*string->p));
        ARET(new_p != NULL);
        string->capacity = new_capacity;
        string->p = new_p;
    }
    return OK;
}

struct Error *string_copy(struct CharBuffer *string, const struct CharBuffer *other)
{
    PRET(string_copy_mem(string, other->p, other->size));
    return OK;
}

struct Error *string_copy_str(struct CharBuffer *string, const char *other)
{
    PRET(string_copy_mem(string, other, strlen(other)));
    return OK;
}

struct Error *string_copy_mem(struct CharBuffer *string, const char *other, size_t other_size)
{
    PRET(string_resize(string, other_size));
    memcpy(string->p, other, other_size);
    return OK;
}

struct Error *string_push(struct CharBuffer *string, char other)
{
    if (string->size + 2 > string->capacity)
    {
        const size_t new_capacity = (string->capacity < 2) ? 2 : (string->capacity * 2);
        char *new_p = realloc(string->p, new_capacity * sizeof(*string->p));
        ARET(new_p != NULL);
        string->capacity = new_capacity;
        string->p = new_p;
    }
    string->p[string->size] = other;
    string->size++;
    string->p[string->size] = '\0';
    return OK;
}

struct Error *string_append(struct CharBuffer *string, const struct CharBuffer *other)
{
    PRET(string_append_mem(string, other->p, other->size));
    return OK;
}

struct Error *string_append_str(struct CharBuffer *string, const char *other)
{
    PRET(string_append_mem(string, other, strlen(other)));
    return OK;
}

struct Error *string_append_mem(struct CharBuffer *string, const char *other, size_t other_size)
{
    const size_t old_size = string->size;
    PRET(string_resize(string, string->size + other_size));
    memcpy(string->p + old_size, other, other_size);
    return OK;
}

struct Error *string_printf(struct CharBuffer *string, const char *format, ...)
{
    va_list va;
    struct Error *error;
    string->size = 0;
    if (string->p != NULL) string->p[0] = '\0';
    va_start(va, format);
    error = string_internal_vprintf_end(string, false, format, va);
    va_end(va);
    PRET(error);
    return OK;
}

struct Error *string_vprintf(struct CharBuffer *string, const char *format, va_list va)
{
    string->size = 0;
    if (string->p != NULL) string->p[0] = '\0';
    PRET(string_internal_vprintf_end(string, false, format, va));
    return OK;
}

struct Error *string_printf_end(struct CharBuffer *string, const char *format, ...)
{
    va_list va;
    struct Error *error;
    va_start(va, format);
    error = string_internal_vprintf_end(string, false, format, va);
    va_end(va);
    PRET(error);
    return OK;
}

struct Error *string_vprintf_end(struct CharBuffer *string, const char *format, va_list va)
{
    PRET(string_internal_vprintf_end(string, false, format, va));
    return OK;
}

static bool string_vprintf_end_internal_reserve(struct CharBuffer *string, size_t estimated_size)
{
    const size_t capacity = string->size + estimated_size;
    if (capacity + 1 > string->capacity)
    {
        char *new_p;
        size_t new_capacity = (string->capacity < 2) ? 2 : string->capacity;
        while (capacity + 1 > new_capacity) new_capacity *= 2;
        new_p = realloc(string->p, new_capacity * sizeof(*string->p));
        if (new_p == NULL) return false;
        string->capacity = new_capacity;
        string->p = new_p;
    }
    return true;
}

static void string_vprintf_end_internal_compact(struct CharBuffer *string)
{
    if (string->size == 0)
    {
        string_finalize(string);
    }
    else
    {
        size_t new_capacity = 1;
        while (string->size + 1 > new_capacity) new_capacity *= 2;
        if (new_capacity < string->capacity)
        {
            char *new_p = realloc(string->p, new_capacity);
            if (new_p != NULL) string->p = new_p;
        }
    }
}

/* #define ENABLE_LONG_LONG */
struct Error *string_internal_vprintf_end(struct CharBuffer *string, bool suppress_errors, const char *format, va_list va)
{
    /*
    Format: %[flags][width][.precision][length]format
    Flags: -, +, ' ', #, 0
    Width: integer or *
    Precision: integer or *
    Lengths: hh, h, l, ll, j, z, t, L
    Formats: c, s, d/i, o, x/X, u, f/F, e/E, a/A, g/G, n, p

    Algorithm:
    1. Find % sequence
    2. Parse sequence
    3. Estimate maximum size and allocate space
    4. Reconstruct sequence
    5. Print to buffer
    */

    while (*format != '\0')
    {
        Flag flags = 0;
        Length length = LENGTH_NONE;
        bool width_present = false, precision_present = false;
        bool width_and_precision_present, width_or_precision_present;
        unsigned int width, precision;
        unsigned int width_or_precision;
        char specifier;
        char format_copy[16]; /* 1 x % + 5 x flags + 2 x * + 2 x length + 1 x specifier + \0 = 12 */
        size_t format_copy_size = 0;
        int printed;
        
        /* Printing substring without percents */
        const char *next_percent = strchr(format, '%');
        const size_t length_without_percent = (next_percent == NULL) ? strlen(format) : (size_t)(next_percent - format);
        if (!string_vprintf_end_internal_reserve(string, length_without_percent)) { va_end(va); return suppress_errors ? PANIC : error_internal_allocate(ERROR_FORMAT()); }
        memcpy(string->p + string->size, format, length_without_percent);
        string->p[string->size + length_without_percent] = '\0';
        string->size += length_without_percent;
        if (next_percent == NULL)
        {
            string_vprintf_end_internal_compact(string);
            break;
        }
        
        /* Parse percent sign */
        format = next_percent + 1;
        if (*format == '%')
        {
            /* Double percent, print % */
            if (!string_vprintf_end_internal_reserve(string, 1)) { va_end(va); return suppress_errors ? PANIC : error_internal_allocate(ERROR_FORMAT()); }
            string->p[string->size] = '%';
            string->p[string->size + 1] = '\0';
            string->size++;
            continue;
        }
        format_copy[0] = '%';

        /* Parse flags */
        while (true)
        {
            Flag new_flags = flags;
            bool flag_present = true;
            switch (*format)
            {
            case '#': new_flags |= FLAG_HASH; break;
            case '0': new_flags |= FLAG_ZERO; break;
            case '-': new_flags |= FLAG_MINUS; break;
            case ' ': new_flags |= FLAG_SPACE; break;
            case '+': new_flags |= FLAG_PLUS; break;
            default: flag_present = false;
            }
            if (!flag_present) break;
            if (new_flags != flags) format_copy[++format_copy_size] = *format;
            flags = new_flags;
            format++;
        }

        /* Parse width */
        if (*format == '*')
        {
            const int signed_width = va_arg(va, int);
            if (signed_width >= 0) width = (unsigned int)signed_width;
            else { width = (unsigned int)-signed_width; flags |= FLAG_MINUS; }
            width_present = true;
            format++;
        }
        else if ('0' <= *format && *format <= '9')
        {
            char *end;
            width = (unsigned int)strtoul(format, &end, 10);
            width_present = true;
            format = end;
        }
        if (width_present) format_copy[++format_copy_size] = '*';

        /* Parse precision */
        if (*format == '.')
        {
            format++;
            if (*format == '*')
            {
                const int signed_precision = va_arg(va, int);
                if (signed_precision >= 0) { precision = (unsigned int)signed_precision; precision_present = true; }
                format++;
            }
            else if ('0' <= *format && *format <= '9')
            {
                char *end;
                precision = (unsigned int)strtoul(format, &end, 10);
                precision_present = true;
                format = end;
            }
        }
        if (precision_present) format_copy[++format_copy_size] = '*';

        /* Parse length */
        switch (*format)
        {
        case 'h': length = (*(format + 1) == 'h') ? LENGTH_SHORT_SHORT : LENGTH_SHORT; break;
        case 'l': length = (*(format + 1) == 'l') ? LENGTH_LONG_LONG : LENGTH_LONG; break;
        case 'j': length = LENGTH_MAX; break;
        case 'z': length = LENGTH_SIZE; break;
        case 't': length = LENGTH_PTRDIFF; break;
        case 'L': length = LENGTH_LONG_LONG; break;
        }
        if (length != LENGTH_NONE) { format_copy[++format_copy_size] = *format; format++; }
        if (length == LENGTH_SHORT_SHORT || length == LENGTH_LONG_LONG) { format_copy[++format_copy_size] = *format; format++; }
        
        /* Parse specifier */
        specifier = *format;
        format_copy[++format_copy_size] = specifier;
        format_copy[++format_copy_size] = '\0';
        format++;

        /* Estimating size */
        width_and_precision_present = width_present && precision_present;
        width_or_precision_present = width_present || precision_present;
        if (width_present) width_or_precision = width;
        if (precision_present) width_or_precision = precision;
        if (specifier == 'c' && length == LENGTH_NONE)
        {
            char *end;
            const char value = (char)va_arg(va, int);
            size_t estimated_size = 1;
            if (width_present && width > estimated_size) estimated_size = width;
            if (!string_vprintf_end_internal_reserve(string, estimated_size)) { va_end(va); return suppress_errors ? PANIC : error_internal_allocate(ERROR_FORMAT()); }
            end = string->p + string->size;
            if (width_and_precision_present) printed = sprintf(end, format_copy, width, precision, value);
            else if (width_or_precision_present) printed = sprintf(end, format_copy, width_or_precision, value);
            else printed = sprintf(end, format_copy, value);
        }
        else if (specifier == 's' && length == LENGTH_NONE)
        {
            char *end;
            const char *value = va_arg(va, const char*);
            const size_t value_length = strlen(value);
            size_t estimated_size = value_length;
            if (width_present && width > estimated_size) estimated_size = width;
            if (!string_vprintf_end_internal_reserve(string, estimated_size)) { va_end(va); return suppress_errors ? PANIC : error_internal_allocate(ERROR_FORMAT()); }
            end = string->p + string->size;
            if (width_and_precision_present) printed = sprintf(end, format_copy, width, precision, value);
            else if (width_or_precision_present) printed = sprintf(end, format_copy, width_or_precision, value);
            else printed = sprintf(end, format_copy, value);
        }
        else if ((specifier == 'd' || specifier == 'i' || specifier == 'o' || specifier == 'x' || specifier == 'X' || specifier == 'u')
            && (length != LENGTH_LONG_DOUBLE))
        {
            /* 3 = log(8)/log(2). Skipping whole logarithm stuff. Also add 8 for safety */
            char *end;
            size_t estimated_size = (8 * sizeof(size_t) / 3 + 8); if (precision_present) estimated_size += precision;
            if (width_present && width > estimated_size) estimated_size = width;
            if (!string_vprintf_end_internal_reserve(string, estimated_size)) { va_end(va); return suppress_errors ? PANIC : error_internal_allocate(ERROR_FORMAT()); }
            end = string->p + string->size;
            if (length == LENGTH_LONG)
            {
                const long value = va_arg(va, long);
                if (width_and_precision_present) printed = sprintf(end, format_copy, width, precision, value);
                else if (width_or_precision_present) printed = sprintf(end, format_copy, width_or_precision, value);
                else printed = sprintf(end, format_copy, value);
            }
            #ifdef ENABLE_LONG_LONG
            else if (length == LENGTH_LONG_LONG)
            {
                const long long value = va_arg(va, long long);
                if (width_and_precision_present) printed = sprintf(end, format_copy, width, precision, value);
                else if (width_or_precision_present) printed = sprintf(end, format_copy, width_or_precision, value);
                else printed = sprintf(end, format_copy, value);
            }
            #endif
            else if (length == LENGTH_MAX)
            {
                const intmax_t value = va_arg(va, intmax_t);
                if (width_and_precision_present) printed = sprintf(end, format_copy, width, precision, value);
                else if (width_or_precision_present) printed = sprintf(end, format_copy, width_or_precision, value);
                else printed = sprintf(end, format_copy, value);
            }
            else if (length == LENGTH_SIZE)
            {
                const size_t value = va_arg(va, size_t);
                if (width_and_precision_present) printed = sprintf(end, format_copy, width, precision, value);
                else if (width_or_precision_present) printed = sprintf(end, format_copy, width_or_precision, value);
                else printed = sprintf(end, format_copy, value);
            }
            else if (length == LENGTH_PTRDIFF)
            {
                const ptrdiff_t value = va_arg(va, ptrdiff_t);
                if (width_and_precision_present) printed = sprintf(end, format_copy, width, precision, value);
                else if (width_or_precision_present) printed = sprintf(end, format_copy, width_or_precision, value);
                else printed = sprintf(end, format_copy, value);
            }
            else
            {
                /* shorts and short shorts also end up here*/
                const int value = va_arg(va, int);
                if (width_and_precision_present) printed = sprintf(end, format_copy, width, precision, value);
                else if (width_or_precision_present) printed = sprintf(end, format_copy, width_or_precision, value);
                else printed = sprintf(end, format_copy, value);
            }
        }
        else if ((specifier == 'f' || specifier == 'F' || specifier == 'e' || specifier == 'E' || specifier == 'g' || specifier == 'G')
            && (length == LENGTH_NONE || length == LENGTH_LONG
            #ifdef ENABLE_LONG_LONG
            || length == LENGTH_LONG_DOUBLE
            #endif
            ))
        {
            /* 512 is WILD overestimate */
            char *end;
            size_t estimated_size = 512 + (precision_present ? precision : 0);
            if (width_present && width > estimated_size) estimated_size = width;
            if (!string_vprintf_end_internal_reserve(string, estimated_size)) { va_end(va); return suppress_errors ? PANIC : error_internal_allocate(ERROR_FORMAT()); }
            end = string->p + string->size;
            #ifdef ENABLE_LONG_LONG
            if (length == LENGTH_LONG_DOUBLE)
            {
                const long double value = va_arg(va, long double);
                if (width_and_precision_present) printed = sprintf(end, format_copy, width, precision, value);
                else if (width_or_precision_present) printed = sprintf(end, format_copy, width_or_precision, value);
                else printed = sprintf(end, format_copy, value);
            }
            else
            #endif
            {
                const double value = va_arg(va, double);
                if (width_and_precision_present) printed = sprintf(end, format_copy, width, precision, value);
                else if (width_or_precision_present) printed = sprintf(end, format_copy, width_or_precision, value);
                else printed = sprintf(end, format_copy, value);
            }
        }
        else if ((specifier == 'n')
            && (length != LENGTH_LONG_DOUBLE))
        {
            if (length == LENGTH_SHORT_SHORT)
            {
                signed char *value = va_arg(va, signed char*);
                *value = (signed char)string->size;
            }
            else if (length == LENGTH_SHORT)
            {
                short *value = va_arg(va, short*);
                *value = (short)string->size;
            }
            else if (length == LENGTH_LONG)
            {
                signed long *value = va_arg(va, long*);
                *value = (long)string->size;
            }
            #ifdef ENABLE_LONG_LONG
            else if (length == LENGTH_LONG_LONG)
            {
                long long *value = va_arg(va, long long*);
                *value = (long long)string->size;
            }
            #endif
            else if (length == LENGTH_MAX)
            {
                intmax_t *value = va_arg(va, intmax_t*);
                *value = (intmax_t)string->size;
            }
            else if (length == LENGTH_SIZE)
            {
                size_t *value = va_arg(va, size_t*);
                *value = (size_t)string->size;
            }
            else if (length == LENGTH_PTRDIFF)
            {
                ptrdiff_t *value = va_arg(va, ptrdiff_t*);
                *value = (ptrdiff_t)string->size;
            }
            else
            {
                int *value = va_arg(va, int*);
                *value = (int)string->size;
            }
            printed = 0;
        }
        else if ((specifier == 'p')
            && (length == LENGTH_NONE))
        {
            char *end;
            const void *value = va_arg(va, const void*);
            const struct CharBuffer *cast = value;
            const char *cast_p = string_get(cast);
            const size_t value_length = cast->size;
            size_t estimated_size = value_length;
            if (width_present && width > estimated_size) estimated_size = width;
            if (!string_vprintf_end_internal_reserve(string, estimated_size)) { va_end(va); return suppress_errors ? PANIC : error_internal_allocate(ERROR_FORMAT()); }
            end = string->p + string->size;
            format_copy[format_copy_size - 1] = 's';
            if (width_and_precision_present) printed = sprintf(end, format_copy, width, precision, cast_p);
            else if (width_or_precision_present) printed = sprintf(end, format_copy, width_or_precision, cast_p);
            else printed = sprintf(end, format_copy, cast_p);
        }
        else { va_end(va); return suppress_errors ? PANIC : error_internal_allocate(ERROR_FORMAT()); }

        if (printed < 0) { va_end(va); return suppress_errors ? PANIC : error_internal_allocate(ERROR_FORMAT()); }
        
        string->size += (size_t)printed;
    }
    va_end(va);
    return OK;
}

void string_trim(struct CharBuffer *string)
{
    /* Count beginning spaces */
    size_t beginning_spaces = 0, ending_spaces = 0, spaces;
    while (true)
    {
        char c;
        if (beginning_spaces == string->size)
        {
            /* The string is all spaces */
            if (string->p != NULL) string->p[0] = '\0';
            string->size = 0;
            return;
        }
        c = string->p[beginning_spaces];
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v')) break;
        beginning_spaces++;
    }
    
    /* Count ending spaces */
    while (true)
    {
        char c = string->p[string->size - ending_spaces - 1];
        if (!(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v')) break;
        ending_spaces++;
    }
    
    /* Move */
    spaces = beginning_spaces + ending_spaces;
    if (beginning_spaces > 0) memmove(string->p, string->p + beginning_spaces, string->size - spaces);
    string->size -= spaces;
    string->p[string->size] = '\0';
}

struct Error *string_remove(struct CharBuffer *string, size_t begin, size_t size)
{
    ARET(begin + size < string->size);
    memmove(string->p + begin, string->p + begin + size, string->size - begin - size + 1);
    string->size -= size;
    return OK;
}

struct Error *string_insert(struct CharBuffer *string, size_t begin, const struct CharBuffer *other)
{
    PRET(string_insert_mem(string, begin, other->p, other->size));
    return OK;
}

struct Error *string_insert_str(struct CharBuffer *string, size_t begin, const char *other)
{
    PRET(string_insert_mem(string, begin, other, strlen(other)));
    return OK;
}

struct Error *string_insert_mem(struct CharBuffer *string, size_t begin, const char *other, size_t other_size)
{
    char *segment_p;
    if (other_size > 0)
    {
        const size_t old_size = string->size;
        const size_t new_size = old_size + other_size;
        PRET(string_resize(string, new_size));
        segment_p = string->p + begin;
        memmove(segment_p + other_size, segment_p, old_size);
    }
    else
    {
        segment_p = string->p + begin;
    }
    memcpy(segment_p, other, other_size);
    return OK;
}

struct Error *string_replace(struct CharBuffer *string, size_t begin, size_t size, const struct CharBuffer *other)
{
    PRET(string_replace_mem(string, begin, size, other->p, other->size));
    return OK;
}

struct Error *string_replace_str(struct CharBuffer *string, size_t begin, size_t size, const char *other)
{
    PRET(string_replace_mem(string, begin, size, other, strlen(other)));
    return OK;
}

struct Error *string_replace_mem(struct CharBuffer *string, size_t begin, size_t size, const char *other, size_t other_size)
{
    char *segment_p;
    ARET(begin + size < string->size);
    if (other_size != size)
    {
        const size_t old_size = string->size;
        const size_t new_size = old_size + other_size - size;
        if (other_size > size)
        {
            /* Expanding */
            PRET(string_resize(string, new_size));
        }
        segment_p = string->p + begin;
        memmove(segment_p + other_size, segment_p + size, old_size - size);
        if (other_size < size)
        {
            /* Shrinking */
            string->p[new_size] = '\0';
            string->size = new_size;
        }
    }
    else
    {
        segment_p = string->p + begin;
    }
    memcpy(segment_p, other, other_size);
    return OK;
}
