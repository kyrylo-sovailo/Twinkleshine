#include "../include/string.h"

#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

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

void string_initialize(struct CharBuffer *string)
{
    const struct CharBuffer zero = ZERO_INIT;
    *string = zero;
}

void string_finalize(struct CharBuffer *string)
{
    const struct CharBuffer zero = ZERO_INIT;
    if (string->p != NULL) free(string->p);
    *string = zero;
}

const cchar_t *string_get(const struct CharBuffer *string)
{
    return (string->p == NULL) ? COMMON_E : string->p;
}

bool string_is_ascii(const struct CharBuffer *string)
{
    return string_is_ascii_mem(string->p, string->size);
}

bool string_is_ascii_str(const cchar_t *string)
{
    return string_is_ascii_mem(string, COMMON_WCS(len(string)));
}

bool string_is_ascii_mem(const cchar_t *string, size_t size)
{
    const cchar_t *p;
    for (p = string; p < string + size; p++)
    {
        if ((*p & (~0x7F)) != 0) return false;
    }
    return true;
}

void string_zero(struct CharBuffer *string)
{
    if (string->p != NULL) string->p[0] = COMMON_L('\0');
    string->size = 0;
}

ERROR_TYPE string_resize(struct CharBuffer *string, size_t size)
{
    if (size + 1 > string->capacity)
    {
        cchar_t *new_p;
        size_t new_capacity = (string->capacity == 0) ? 1 : string->capacity;
        while (size + 1 > new_capacity) new_capacity *= 2;
        new_p = (cchar_t*)realloc(string->p, new_capacity * sizeof(*string->p));
        ARET(new_p != NULL);
        string->capacity = new_capacity;
        string->p = new_p;
    }
    string->size = size;
    string->p[size] = COMMON_L('\0');
    ERROR_RETURN_OK();
}

ERROR_TYPE string_reserve(struct CharBuffer *string, size_t capacity)
{
    if (capacity + 1 > string->capacity)
    {
        cchar_t *new_p;
        size_t new_capacity = (string->capacity == 0) ? 1 : string->capacity;
        while (capacity + 1 > new_capacity) new_capacity *= 2;
        new_p = (cchar_t*)realloc(string->p, new_capacity * sizeof(*string->p));
        ARET(new_p != NULL);
        string->capacity = new_capacity;
        string->p = new_p;
    }
    ERROR_RETURN_OK();
}

ERROR_TYPE string_copy(struct CharBuffer *string, const struct CharBuffer *other)
{
    PRET(string_copy_mem(string, other->p, other->size));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_copy_str(struct CharBuffer *string, const cchar_t *other)
{
    PRET(string_copy_mem(string, other, COMMON_WCS(len(other))));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_copy_mem(struct CharBuffer *string, const cchar_t *other, size_t other_size)
{
    PRET(string_resize(string, other_size));
    COMMON_W(memcpy(string->p, other, other_size));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_push(struct CharBuffer *string, cchar_t other)
{
    if (string->size + 2 > string->capacity)
    {
        const size_t new_capacity = (string->capacity == 0) ? 1 : (string->capacity * 2);
        cchar_t *new_p = (cchar_t*)realloc(string->p, new_capacity * sizeof(*string->p));
        ARET(new_p != NULL);
        string->capacity = new_capacity;
        string->p = new_p;
    }
    string->p[string->size] = other;
    string->size++;
    string->p[string->size] = COMMON_L('\0');
    ERROR_RETURN_OK();
}

ERROR_TYPE string_append(struct CharBuffer *string, const struct CharBuffer *other)
{
    PRET(string_append_mem(string, other->p, other->size));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_append_str(struct CharBuffer *string, const cchar_t *other)
{
    PRET(string_append_mem(string, other, COMMON_WCS(len(other))));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_append_mem(struct CharBuffer *string, const cchar_t *other, size_t other_size)
{
    const size_t old_size = string->size;
    PRET(string_resize(string, string->size + other_size));
    COMMON_W(memcpy(string->p + old_size, other, other_size));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_print(struct CharBuffer *string, const cchar_t *format, ...)
{
    va_list va;
    ERROR_DECLARE();
    va_start(va, format);
    ERROR_ASSIGN(string_vprint(string, format, va));
    va_end(va);
    ERROR_RETURN();
}

ERROR_TYPE string_vprint(struct CharBuffer *string, const cchar_t *format, va_list va)
{
    string_zero(string);
    PRET(string_vprint_append(string, format, va));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_print_append(struct CharBuffer *string, const cchar_t *format, ...)
{
    va_list va;
    ERROR_DECLARE();
    va_start(va, format);
    ERROR_ASSIGN(string_vprint_append(string, format, va));
    va_end(va);
    ERROR_RETURN();
}

ERROR_TYPE string_vprint_append(struct CharBuffer *string, const cchar_t *format, va_list va)
{
    PRET(string_internal_vprint_append(string, false, format, va));
    ERROR_RETURN_OK();
}

static bool string_vprint_append_internal_reserve(struct CharBuffer *string, size_t estimated_size)
{
    const size_t capacity = string->size + estimated_size;
    if (capacity + 1 > string->capacity)
    {
        cchar_t *new_p;
        size_t new_capacity = (string->capacity == 0) ? 1 : string->capacity;
        while (capacity + 1 > new_capacity) new_capacity *= 2;
        new_p = (cchar_t*)realloc(string->p, new_capacity * sizeof(*string->p));
        if (new_p == NULL) return false;
        string->capacity = new_capacity;
        string->p = new_p;
    }
    return true;
}

static void string_vprint_append_internal_compact(struct CharBuffer *string)
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
            cchar_t *new_p = (cchar_t*)realloc(string->p, new_capacity * sizeof(*string->p));
            if (new_p != NULL) { string->capacity = new_capacity; string->p = new_p; }
        }
    }
}

/*
Switches:
ENABLE_LONG_LONG - enables printing of "long long" which itself is an extension and won't even compile under C89
ENABLE_EXTENSIONS - enables lengths hh, j, z, t, and formats a, A, and F, and may or may not run under C89
ENABLE_LONG_CHAR - enables lc and ls which may or may and compile, and may or may not run under C89
*/
/* #define ENABLE_LONG_LONG */
#define ENABLE_EXTENSIONS
#define ENABLE_LONG_CHAR

#ifdef COMMON_WCHAR
    #define END(STRING) (STRING->p + STRING->size), (size_t)(STRING->capacity - STRING->size)
#else
    #define END(STRING) (STRING->p + STRING->size)
#endif

#if defined(ERROR_DIE)
    #define SOFT_RET() { if (!suppress_errors) error_internal_print_die(ERROR_FORMAT()); return; }
    #define SOFT_ARET(EXPRESSION) { const bool check = EXPRESSION; if (check) {} else { if (!suppress_errors) error_internal_print_die(ERROR_FORMAT()); return; } }
#elif defined(ERROR_PRINT)
    #define SOFT_RET() { if (!suppress_errors) error_internal_print(ERROR_FORMAT()); return false; }
    #define SOFT_ARET(EXPRESSION) { const bool check = EXPRESSION; if (check) {} else { if (!suppress_errors) error_internal_print(ERROR_FORMAT_E(#EXPRESSION)); return false; } }
#elif defined(ERROR_TRACE)
    #define SOFT_RET() { if (!suppress_errors) return error_internal_allocate(ERROR_FORMAT()); return PANIC; }
    #define SOFT_ARET(EXPRESSION) { const bool check = EXPRESSION; if (check) {} else { if (!suppress_errors) return error_internal_allocate(ERROR_FORMAT_E(#EXPRESSION)); return PANIC; } }
#endif

ERROR_TYPE string_internal_vprint_append(struct CharBuffer *string, bool suppress_errors, const cchar_t *format, va_list va)
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

    while (*format != COMMON_L('\0'))
    {
        Flag flags = 0;
        Length length = LENGTH_NONE;
        const unsigned int width_invalid = 0; unsigned int width = width_invalid;
        const unsigned int precision_invalid = (unsigned int)-1; unsigned int precision = precision_invalid;
        cchar_t specifier;
        cchar_t format_copy[16]; /* 1 x % + 5 x flags + 2 x * + 2 x length + 1 x specifier + \0 = 12 */
        size_t format_copy_size = 0;
        int printed;
        
        /* Printing substring without percents */
        const cchar_t *next_percent = COMMON_WCS(chr(format, COMMON_L('%')));
        const size_t length_without_percent = (next_percent == NULL) ? COMMON_WCS(len(format)) : (size_t)(next_percent - format);
        SOFT_ARET(string_vprint_append_internal_reserve(string, length_without_percent));
        COMMON_W(memcpy(string->p + string->size, format, length_without_percent));
        string->p[string->size + length_without_percent] = COMMON_L('\0');
        string->size += length_without_percent;
        if (next_percent == NULL)
        {
            string_vprint_append_internal_compact(string);
            break;
        }
        
        /* Parse percent sign */
        format = next_percent + 1;
        if (*format == COMMON_L('%'))
        {
            /* Double percent, print % */
            SOFT_ARET(string_vprint_append_internal_reserve(string, 1));
            string->p[string->size] = COMMON_L('%');
            string->p[string->size + 1] = COMMON_L('\0');
            string->size++;
            continue;
        }
        format_copy[0] = COMMON_L('%');

        /* Parse flags */
        while (true)
        {
            Flag new_flags = flags;
            bool flag_present = true;
            switch (*format)
            {
            case COMMON_L('#'): new_flags |= FLAG_HASH; break;
            case COMMON_L('0'): new_flags |= FLAG_ZERO; break;
            case COMMON_L('-'): new_flags |= FLAG_MINUS; break;
            case COMMON_L(' '): new_flags |= FLAG_SPACE; break;
            case COMMON_L('+'): new_flags |= FLAG_PLUS; break;
            default: flag_present = false;
            }
            if (!flag_present) break;
            if (new_flags != flags) format_copy[++format_copy_size] = *format;
            flags = new_flags;
            format++;
        }

        /* Parse width */
        if (*format == COMMON_L('*'))
        {
            const int signed_width = va_arg(va, int);
            if (signed_width >= 0) width = (unsigned int)signed_width;
            else { width = (unsigned int)-signed_width; flags |= FLAG_MINUS; }
            format++;
        }
        else if (COMMON_L('0') <= *format && *format <= COMMON_L('9'))
        {
            cchar_t *end;
            width = (unsigned int)COMMON_WCS(toul(format, &end, 10));
            format = end;
        }
        format_copy[++format_copy_size] = COMMON_L('*');

        /* Parse precision */
        if (*format == COMMON_L('.'))
        {
            format++;
            if (*format == COMMON_L('*'))
            {
                const int signed_precision = va_arg(va, int);
                if (signed_precision >= 0) { precision = (unsigned int)signed_precision; }
                format++;
            }
            else if (COMMON_L('0') <= *format && *format <= COMMON_L('9'))
            {
                cchar_t *end;
                precision = (unsigned int)COMMON_WCS(toul(format, &end, 10));
                format = end;
            }
        }
        if (precision != precision_invalid) { format_copy[++format_copy_size] = COMMON_L('.'); format_copy[++format_copy_size] = COMMON_L('*'); }

        /* Parse length */
        switch (*format)
        {
        case 'h': length = (*(format + 1) == COMMON_L('h')) ? LENGTH_SHORT_SHORT : LENGTH_SHORT; break;
        case 'l': length = (*(format + 1) == COMMON_L('l')) ? LENGTH_LONG_LONG : LENGTH_LONG; break;
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
        format_copy[++format_copy_size] = COMMON_L('\0');
        format++;

        /* Print */
        if (specifier == COMMON_L('c') && length == LENGTH_NONE)
        {
            #ifdef COMMON_WCHAR
                /* nchar_t -> wchar_t */
                wchar_t value;
                size_t estimated_size = 1;
                const nchar_t nc = (nchar_t)va_arg(va, /* nchar_t */ int);
                format_copy[format_copy_size - 1] = COMMON_L('l');
                format_copy[format_copy_size    ] = COMMON_L('c');
                format_copy[format_copy_size + 1] = COMMON_L('\0');
                PRET(string_internal_to_wstring(&nc, 1, &value, &estimated_size, true));
            #else
                const nchar_t value = (nchar_t)va_arg(va, /* nchar_t */ int);
                size_t estimated_size = 1;
            #endif
            if (width > estimated_size) estimated_size = width;
            SOFT_ARET(string_vprint_append_internal_reserve(string, estimated_size));
            if (precision != precision_invalid) printed = COMMON_SW(printf(END(string), format_copy, width, precision, value));
            else printed = COMMON_SW(printf(END(string), format_copy, width, value));
        }
        #ifdef ENABLE_LONG_CHAR
        else if (specifier == COMMON_L('c') && length == LENGTH_LONG)
        {
            #ifdef COMMON_WCHAR
                const wchar_t value = (wchar_t)va_arg(va, /* wchar_t */ int);
                size_t estimated_size = 1;
            #else
                /* wchar_t -> nchar_t */
                nchar_t value[8];
                size_t estimated_size;
                const wchar_t wc = (wchar_t)va_arg(va, /* wchar_t */ int);
                format_copy[format_copy_size - 1] = COMMON_L('s');
                format_copy[format_copy_size    ] = COMMON_L('\0');
                PRET(string_internal_to_nstring(&wc, 1, &value[0], &estimated_size, true));
                value[estimated_size] = '\0';
            #endif
            if (width > estimated_size) estimated_size = width;
            SOFT_ARET(string_vprint_append_internal_reserve(string, estimated_size));
            if (precision != precision_invalid) printed = COMMON_SW(printf(END(string), format_copy, width, precision, value));
            else printed = COMMON_SW(printf(END(string), format_copy, width, value));
        }
        #endif
        else if (specifier == COMMON_L('s') && length == LENGTH_NONE)
        {
            bool reserve;
            #ifdef COMMON_WCHAR
                /* nchar_t -> wchar_t */
                wchar_t *value;
                size_t estimated_size;
                const nchar_t *np = va_arg(va, const nchar_t*);
                size_t nsize; /* Precision assumed to apply to original encoding, target assumed to apply to target encoding */
                if (precision != precision_invalid) { const nchar_t *null = (nchar_t*)memchr(np, '\0', precision); nsize = (null != NULL) ? (size_t)(null - np) : precision; }
                else nsize = strlen(np);
                PRET(string_internal_to_wstring(np, nsize, NULL, &estimated_size, true));
                value = (wchar_t*)malloc((estimated_size + 1) * sizeof(*value));
                SOFT_ARET(value != NULL);
                value[estimated_size] = '\0';
                PIGNORE(string_internal_to_wstring(np, nsize, value, &estimated_size, true));
            #else
                const nchar_t *value = va_arg(va, const nchar_t*);
                size_t estimated_size = (precision != precision_invalid) ? precision : strlen(value);    
            #endif
            if (width > estimated_size) estimated_size = width;
            reserve = string_vprint_append_internal_reserve(string, estimated_size);
            #ifdef COMMON_WCHAR
                if (!reserve) free(value);
            #endif
            SOFT_ARET(reserve);
            if (precision != precision_invalid) printed = COMMON_SW(printf(END(string), format_copy, width, precision, value));
            else printed = COMMON_SW(printf(END(string), format_copy, width, value));
            #ifdef COMMON_WCHAR
                free(value);
            #endif
        }
        #ifdef ENABLE_LONG_CHAR
        else if (specifier == COMMON_L('s') && length == LENGTH_LONG)
        {
            bool reserve;
            #ifdef COMMON_WCHAR
                const wchar_t *value = va_arg(va, const wchar_t*);
                size_t estimated_size = (precision != precision_invalid) ? precision : wcslen(value);
            #else
                /* wchar_t -> nchar_t */
                nchar_t *value;
                size_t estimated_size;
                const wchar_t *wp = va_arg(va, const wchar_t*);
                size_t wsize; /* Precision assumed to apply to original encoding, target assumed to apply to target encoding */
                if (precision != precision_invalid) { const wchar_t *null = wmemchr(wp, '\0', precision); wsize = (null != NULL) ? (size_t)(null - wp) : precision; }
                else wsize = wcslen(wp);
                PRET(string_internal_to_nstring(wp, wsize, NULL, &estimated_size, true));
                value = (nchar_t*)malloc((estimated_size + 1) * sizeof(*value));
                SOFT_ARET(value != NULL);
                value[estimated_size] = '\0';
                PIGNORE(string_internal_to_nstring(wp, wsize, value, &estimated_size, true));
            #endif
            if (width > estimated_size) estimated_size = width;
            reserve = string_vprint_append_internal_reserve(string, estimated_size);
            #ifndef COMMON_WCHAR
                if (!reserve) free(value);
            #endif
            SOFT_ARET(reserve);
            if (precision != precision_invalid) printed = COMMON_SW(printf(END(string), format_copy, width, precision, value));
            else printed = COMMON_SW(printf(END(string), format_copy, width, value));
            #ifndef COMMON_WCHAR
                free(value);
            #endif
        }
        #endif
        else if ((specifier == COMMON_L('d') || specifier == COMMON_L('i') || specifier == COMMON_L('o') || specifier == COMMON_L('x') || specifier == COMMON_L('X') || specifier == COMMON_L('u'))
            && (length == LENGTH_NONE || length == LENGTH_SHORT || length == LENGTH_LONG
            #ifdef ENABLE_LONG_LONG
            || length == LENGTH_LONG_LONG
            #endif
            #ifdef ENABLE_EXTENSIONS
            || length == LENGTH_SHORT_SHORT || length == LENGTH_MAX || length == LENGTH_SIZE || length == LENGTH_PTRDIFF
            #endif
            ))
        {
            /* 3 = log(8)/log(2). Skipping whole logarithm stuff. Also add 8 for safety */
            size_t estimated_size = (8 * sizeof(size_t) / 3 + 8); if (precision != precision_invalid) estimated_size += precision; /* Avoiding GCC bug */
            if (width > estimated_size) estimated_size = width;
            SOFT_ARET(string_vprint_append_internal_reserve(string, estimated_size));
            if (length == LENGTH_LONG)
            {
                const long value = va_arg(va, long);
                if (precision != precision_invalid) printed = COMMON_SW(printf(END(string), format_copy, width, precision, value));
                else printed = COMMON_SW(printf(END(string), format_copy, width, value));
            }
            #ifdef ENABLE_LONG_LONG
            else if (length == LENGTH_LONG_LONG)
            {
                const long long value = va_arg(va, long long);
                if (precision != precision_invalid) printed = COMMON_SW(printf(END(string), format_copy, width, precision, value));
                else printed = COMMON_SW(printf(END(string), format_copy, width, value));
            }
            #endif
            #ifdef ENABLE_EXTENSIONS
            else if (length == LENGTH_MAX)
            {
                const intmax_t value = va_arg(va, intmax_t);
                if (precision != precision_invalid) printed = COMMON_SW(printf(END(string), format_copy, width, precision, value));
                else printed = COMMON_SW(printf(END(string), format_copy, width, value));
            }
            else if (length == LENGTH_SIZE)
            {
                const size_t value = va_arg(va, size_t);
                if (precision != precision_invalid) printed = COMMON_SW(printf(END(string), format_copy, width, precision, value));
                else printed = COMMON_SW(printf(END(string), format_copy, width, value));
            }
            else if (length == LENGTH_PTRDIFF)
            {
                const ptrdiff_t value = va_arg(va, ptrdiff_t);
                if (precision != precision_invalid) printed = COMMON_SW(printf(END(string), format_copy, width, precision, value));
                else printed = COMMON_SW(printf(END(string), format_copy, width, value));
            }
            #endif
            else
            {
                /* shorts and short shorts also end up here*/
                const int value = va_arg(va, int);
                if (precision != precision_invalid) printed = COMMON_SW(printf(END(string), format_copy, width, precision, value));
                else printed = COMMON_SW(printf(END(string), format_copy, width, value));
            }
        }
        else if ((specifier == COMMON_L('f') || specifier == COMMON_L('e') || specifier == COMMON_L('E') || specifier == COMMON_L('g') || specifier == COMMON_L('G')
            #ifdef ENABLE_EXTENSIONS
            || specifier == COMMON_L('F') || specifier == COMMON_L('a') || specifier == COMMON_L('A')
            #endif
            )
            && (length == LENGTH_NONE || length == LENGTH_LONG_DOUBLE))
        {
            /* 512 is a WILD overestimate */
            size_t estimated_size = 512; if (precision != precision_invalid) estimated_size += precision;
            if (width > estimated_size) estimated_size = width;
            SOFT_ARET(string_vprint_append_internal_reserve(string, estimated_size));
            if (length == LENGTH_LONG_DOUBLE)
            {
                const long double value = va_arg(va, long double);
                if (precision != precision_invalid) printed = COMMON_SW(printf(END(string), format_copy, width, precision, value));
                else printed = COMMON_SW(printf(END(string), format_copy, width, value));
            }
            else
            {
                const double value = va_arg(va, double);
                if (precision != precision_invalid) printed = COMMON_SW(printf(END(string), format_copy, width, precision, value));
                else printed = COMMON_SW(printf(END(string), format_copy, width, value));
            }
        }
        else if (specifier == COMMON_L('n')
            && (length == LENGTH_NONE || length == LENGTH_SHORT || length == LENGTH_LONG
            #ifdef ENABLE_LONG_LONG
            || length == LENGTH_LONG_LONG
            #endif
            #ifdef ENABLE_EXTENSIONS
            || length == LENGTH_SHORT_SHORT || length == LENGTH_MAX || length == LENGTH_SIZE || length == LENGTH_PTRDIFF
            #endif
            ))
        {
            if (length == LENGTH_SHORT)
            {
                short *value = va_arg(va, short*);
                *value = (short)string->size;
            }
            else if (length == LENGTH_LONG)
            {
                long *value = va_arg(va, long*);
                *value = (long)string->size;
            }
            #ifdef ENABLE_LONG_LONG
            else if (length == LENGTH_LONG_LONG)
            {
                long long *value = va_arg(va, long long*);
                *value = (long long)string->size;
            }
            #endif
            #ifdef ENABLE_EXTENSIONS
            else if (length == LENGTH_SHORT_SHORT)
            {
                signed char *value = va_arg(va, signed char*);
                *value = (signed char)string->size;
            }
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
            #endif
            else
            {
                int *value = va_arg(va, int*);
                *value = (int)string->size;
            }
            printed = 0;
        }
        else if (specifier == COMMON_L('p') && length == LENGTH_NONE)
        {
            const void *value = va_arg(va, const void*);
            const struct CharBuffer *cast = (const struct CharBuffer*)value;
            const cchar_t *cast_p = string_get(cast);
            const size_t value_length = cast->size;
            size_t estimated_size = value_length;
            if (width > estimated_size) estimated_size = width;
            SOFT_ARET(string_vprint_append_internal_reserve(string, estimated_size));
            #ifdef COMMON_WCHAR
                format_copy[format_copy_size - 1] = COMMON_L('l');
                format_copy[format_copy_size    ] = COMMON_L('s');
                format_copy[format_copy_size + 1] = COMMON_L('\0');
            #else
                format_copy[format_copy_size - 1] = COMMON_L('s');
            #endif
            if (precision != precision_invalid) printed = COMMON_SW(printf(END(string), format_copy, width, precision, cast_p));
            else printed = COMMON_SW(printf(END(string), format_copy, width, cast_p));
        }
        else SOFT_RET();
        
        SOFT_ARET(printed >= 0);
        string->size += (size_t)printed;
    }
    ERROR_RETURN_OK();
}

void string_trim(struct CharBuffer *string)
{
    /* Count beginning spaces */
    size_t beginning_spaces = 0, ending_spaces = 0, spaces;
    while (true)
    {
        cchar_t c;
        if (beginning_spaces == string->size)
        {
            /* The string is all spaces */
            string_zero(string);
            return;
        }
        c = string->p[beginning_spaces];
        if (!(c == COMMON_L(' ') || c == COMMON_L('\t') || c == COMMON_L('\n') || c == COMMON_L('\r') || c == COMMON_L('\v'))) break;
        beginning_spaces++;
    }
    
    /* Count ending spaces */
    while (true)
    {
        cchar_t c = string->p[string->size - ending_spaces - 1];
        if (!(c == COMMON_L(' ') || c == COMMON_L('\t') || c == COMMON_L('\n') || c == COMMON_L('\r') || c == COMMON_L('\v'))) break;
        ending_spaces++;
    }
    
    /* Move */
    spaces = beginning_spaces + ending_spaces;
    if (beginning_spaces > 0) COMMON_W(memmove(string->p, string->p + beginning_spaces, string->size - spaces));
    string->size -= spaces;
    string->p[string->size] = COMMON_L('\0');
}

ERROR_TYPE string_remove(struct CharBuffer *string, size_t begin, size_t size)
{
    ARET(begin + size < string->size);
    COMMON_W(memmove(string->p + begin, string->p + begin + size, string->size - begin - size + 1));
    string->size -= size;
    ERROR_RETURN_OK();
}

ERROR_TYPE string_insert(struct CharBuffer *string, size_t begin, const struct CharBuffer *other)
{
    PRET(string_insert_mem(string, begin, other->p, other->size));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_insert_str(struct CharBuffer *string, size_t begin, const cchar_t *other)
{
    PRET(string_insert_mem(string, begin, other, COMMON_WCS(len(other))));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_insert_mem(struct CharBuffer *string, size_t begin, const cchar_t *other, size_t other_size)
{
    cchar_t *segment_p;
    if (other_size > 0)
    {
        const size_t old_size = string->size;
        const size_t new_size = old_size + other_size;
        PRET(string_resize(string, new_size));
        segment_p = string->p + begin;
        COMMON_W(memmove(segment_p + other_size, segment_p, old_size));
    }
    else
    {
        segment_p = string->p + begin;
    }
    COMMON_W(memcpy(segment_p, other, other_size));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_replace(struct CharBuffer *string, size_t begin, size_t size, const struct CharBuffer *other)
{
    PRET(string_replace_mem(string, begin, size, other->p, other->size));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_replace_str(struct CharBuffer *string, size_t begin, size_t size, const cchar_t *other)
{
    PRET(string_replace_mem(string, begin, size, other, COMMON_WCS(len(other))));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_replace_mem(struct CharBuffer *string, size_t begin, size_t size, const cchar_t *other, size_t other_size)
{
    cchar_t *segment_p;
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
        COMMON_W(memmove(segment_p + other_size, segment_p + size, old_size - size));
        if (other_size < size)
        {
            /* Shrinking */
            string->p[new_size] = COMMON_L('\0');
            string->size = new_size;
        }
    }
    else
    {
        segment_p = string->p + begin;
    }
    COMMON_W(memcpy(segment_p, other, other_size));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_to_wstring(const nchar_t *np, size_t nsize, wchar_t *wp, size_t *wsize)
{
    PRET(string_internal_to_wstring(np, nsize, wp, wsize, false));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_to_nstring(const wchar_t *wp, size_t wsize, nchar_t *np, size_t *nsize)
{
    PRET(string_internal_to_nstring(wp, wsize, np, nsize, false));
    ERROR_RETURN_OK();
}

ERROR_TYPE string_internal_to_wstring(const nchar_t* np, size_t nsize, wchar_t* wp, size_t* wsize, bool suppress_errors)
{
    *wsize = 0;
    while (nsize > 0)
    {
        /* Decode UTF-8 */
        const unsigned int nc = (unsigned int)*np;
        size_t symbol_size;
        unsigned int code;
        if ((nc & 0x80) == 0)
        {
            code = nc;
            symbol_size = 1;
        }
        else if ((nc & 0xE0) == 0xC0)
        {
            SOFT_ARET(nsize >= 2 && ((unsigned int)np[1] & 0xC0) == 0x80);
            code = (unsigned int)(((nc & 0x1F) << 6) | ((unsigned int)np[1] & 0x3F));
            symbol_size = 2;
        }
        else if ((nc & 0xF0) == 0xE0)
        {
            SOFT_ARET(nsize >= 3 && ((unsigned int)np[1] & 0xC0) == 0x80 && ((unsigned int)np[2] & 0xC0) == 0x80);
            code = (unsigned int)(((nc & 0x0F) << 12) | (((unsigned int)np[1] & 0x3F) << 6) | ((unsigned int)np[2] & 0x3F));
            symbol_size = 3;
        }
        else if ((nc & 0xF8) == 0xF0)
        {
            SOFT_ARET(nsize >= 4 && ((unsigned int)np[1] & 0xC0) == 0x80 && ((unsigned int)np[2] & 0xC0) == 0x80 && ((unsigned int)np[3] & 0xC0) == 0x80);
            code = (unsigned int)(((nc & 0x07) << 18) | (((unsigned int)np[1] & 0x3F) << 12) | (((unsigned int)np[2] & 0x3F) << 6) | ((unsigned int)np[3] & 0x3F));
            symbol_size = 4;
        }
        else SOFT_RET();
        np += symbol_size;
        nsize -= symbol_size;

        if (sizeof(wchar_t) > 2)
        {
            /* Encode UTF-32 */
            if (wp != NULL) *wp = (wchar_t)code;
            symbol_size = 1;
        }
        else
        {
            /* Encode UTF-16 */
            SOFT_ARET(code < 0xE000 || (code >= 0xD800 && code < 0x110000));
            if (code < 0x10000)
            {
                if (wp != NULL) *wp = (wchar_t)code;
                symbol_size = 1;
            }
            else
            {
                if (wp != NULL)
                {
                    wp[0] = ((code - 0x10000) >> 10) & 0x3FF;
                    wp[1] = (code - 0x10000) & 0x3FF;
                }
                symbol_size = 2;
            }
        }
        if (wp != NULL) wp += symbol_size;
        *wsize += 1;
    }
    ERROR_RETURN_OK();
}

ERROR_TYPE string_internal_to_nstring(const wchar_t* wp, size_t wsize, nchar_t* np, size_t* nsize, bool suppress_errors)
{
    *nsize = 0;
    while (wsize > 0)
    {
        const unsigned int wc = (unsigned int)*wp;
        unsigned int code;
        size_t symbol_size;
        if (sizeof(wchar_t) > 2)
        {
            /* Decode UTF-32 */
            code = wc;
            symbol_size = 1;
        }
        else
        {
            /* Decode UTF-16 */
            if (!(wc >= 0xD800 && wc < 0xE000))
            {
                code = (unsigned int)*wp;
                symbol_size = 1;
            }
            else if (wc >= 0xD800 && wc < 0xDC00)
            {
                SOFT_ARET(wsize >= 2 && ((unsigned int)wp[1] >= 0xDC00 && (unsigned int)wp[1] < 0xE000));
                code = (((wc - 0xD800) << 10) | ((unsigned int)wp[1] - 0xDC00)) + 0x10000;
                symbol_size = 2;
            }
            else RET0("Invalid UTF-16 symbol");
        }
        wp += symbol_size;
        wsize -= symbol_size;

        /* Encode UTF-8 */
        ARET(code < 0x110000);
        if (code < 0x80)
        {
            if (np != NULL) *np = (nchar_t)code;
            symbol_size = 1;
        }
        else if (code < 0x800)
        {
            if (np != NULL)
            {
                np[0] = (nchar_t)(0xC0 | ((code >> 6) & 0x1F));
                np[1] = (nchar_t)(0x80 | (code & 0x3F));
            }
            symbol_size = 2;
        }
        else if (code < 0x10000)
        {
            if (np != NULL)
            {
                np[0] = (nchar_t)(0xE0 | ((code >> 12) & 0x0F));
                np[1] = (nchar_t)(0x80 | ((code >> 6) & 0x3F));
                np[2] = (nchar_t)(0x80 | (code & 0x3F));
            }
            symbol_size = 3;
        }
        else
        {
            if (np != NULL)
            {
                np[0] = (nchar_t)(0xE0 | ((code >> 18) & 0x07));
                np[1] = (nchar_t)(0x80 | ((code >> 12) & 0x3F));
                np[2] = (nchar_t)(0x80 | ((code >> 6) & 0x3F));
                np[3] = (nchar_t)(0x80 | (code & 0x3F));
            }
            symbol_size = 4;
        }
        if (np != NULL) np += symbol_size;
        *nsize += symbol_size;
    }
    ERROR_RETURN_OK();
}
