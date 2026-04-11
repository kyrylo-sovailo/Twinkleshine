#include "../include/path.h"
#include "../include/string.h"

#ifdef WIN32
    #include <Windows.h>
#else
    #include <unistd.h>
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

struct CharBuffer g_application = ZERO_INIT;
struct CharBuffer g_directory = ZERO_INIT;
struct CharBuffer g_working_directory = ZERO_INIT;

#ifdef WIN32
    #define PATH_SEPARATOR '\\'
    #define PATH_SEPARATOR_STR "\\"
#else
    #define PATH_SEPARATOR '/'
    #define PATH_SEPARATOR_STR "/"
#endif

static const char *path_next_separator(const char *p, size_t size)
{
    #ifdef WIN32
        const char *found = NULL;
        const char *copy = p;
        for (copy = p; copy < p + size; copy++)
        {
            if (*copy == '/' || *copy == '\\') { found = copy; break; }
        }
        return found;
    #else
        return (const char*)memchr(p, '/', size);
    #endif
}

static size_t path_root_size(const char *p, size_t size)
{
    #ifdef WIN32
        /* Path like C:\ */
        if (size >= 3
        && ((p[0] >= 'a' && p[0] <= 'z') || (p[0] >= 'A' && p[0] <= 'Z'))
        && p[1] == ':'
        && (p[2] == '/' || p[2] == '\\'))
        {
            return 3;
        }

        /* Path like \\.\ */
        else if (size >= 2
        && (p[0] == '/' || p[0] == '\\')
        && (p[1] == '/' || p[1] == '\\'))
        {
            const char *last_separator = path_next_separator(p + 2, size - 2);
            if (last_separator == NULL) return 2; /* Shouldn't happen actually */
            else return (size_t)(last_separator - p) + 1;
        }
    #else
        if (size >= 1 && p[0] == '/') return 1;
    #endif
    return 0;
}


static ERROR_TYPE path_append_mem_noroot(struct CharBuffer *path, const char *other, size_t other_size) NODISCARD;
static ERROR_TYPE path_append_mem_noroot(struct CharBuffer *path, const char *other, size_t other_size)
{
    const char *p = other;
    size_t size = other_size;
    while (true)
    {
        const char *found = path_next_separator(p, size);
        const size_t part_size = (found == NULL) ? size : (size_t)(found - p);
        if (part_size == 0)
        {
            /* Double slash, do nothing */
        }
        else if (part_size == 1 && memcmp(p, ".", 1) == 0)
        {
            /* Dot, do nothing */
        }
        else if (part_size == 2 && memcmp(p, "..", 2) == 0)
        {
            /* Double dot, step back */
            PRET(path_get_directory(path, path, true));
        }
        else
        {
            /* Regular case */
            if (path->size > 0 && path->p[path->size - 1] != PATH_SEPARATOR) PRET(string_push(path, PATH_SEPARATOR)); /* Protection against root */
            PRET(string_append_mem(path, p, part_size));
        }
        if (found == NULL) break;
        p = found + 1;
        size = size - part_size - 1;
    }
    ERROR_RETURN_OK();
}

ERROR_TYPE path_module_initialize(int argc, char **argv)
{
    PRET(path_get_executable_path(&g_application, argc, argv));
    PRET(path_get_directory(&g_directory, &g_application, true));
    PRET(path_get_basename(&g_application, &g_application));
    PRET(path_get_working_directory(&g_working_directory));
    ERROR_RETURN_OK();
}

void path_module_finalize(void)
{
    string_finalize(&g_application);
    string_finalize(&g_directory);
    string_finalize(&g_working_directory);
}

ERROR_TYPE path_copy(struct CharBuffer *path, const struct CharBuffer *other)
{
    /* Special form, assuming other is formatted */
    PRET(string_copy_mem(path, other->p, other->size));
    ERROR_RETURN_OK();
}

ERROR_TYPE path_copy_str(struct CharBuffer *path, const char *other)
{
    PRET(path_copy_mem(path, other, strlen(other)));
    ERROR_RETURN_OK();
}

ERROR_TYPE path_copy_mem(struct CharBuffer *path, const char *other, size_t other_size)
{
    const size_t root_size = path_root_size(other, other_size);
    #ifdef WIN32
        size_t i;
        PRET(string_resize(path, root_size));
        for (i = 0; i < root_size; i++) path->p[i] = (other[i] == '/') ? '\\' : other[i];
    #else
        PRET(string_copy_mem(path, other, root_size));
    #endif
    PRET(path_append_mem_noroot(path, other + root_size, other_size - root_size));
    ERROR_RETURN_OK();
}

ERROR_TYPE path_append(struct CharBuffer *path, const struct CharBuffer *other)
{
    /* Special form, assuming other is formatted */
    const char *p = other->p;
    size_t size = other->size;
    ARET(!path_absolute(other));
    while (true)
    {
        const char *found = path_next_separator(p, size);
        const size_t part_size = (found == NULL) ? size : (size_t)(found - p);
        if (part_size == 2 && memcmp(p, "..", 2) == 0)
        {
            /* Double dot, step back */
            PRET(path_get_directory(path, path, true));
        }
        else
        {
            /* Regular case, append everything and quit */
            if (path->size > 0) PRET(string_push(path, PATH_SEPARATOR));
            PRET(string_append_mem(path, p, size));
            break;
        }
        if (found == NULL) break;
        p = found + 1;
        size = size - part_size - 1;
    }
    ERROR_RETURN_OK();
}

ERROR_TYPE path_append_str(struct CharBuffer *path, const char *other)
{
    PRET(path_append_mem(path, other, strlen(other)));
    ERROR_RETURN_OK();
}

ERROR_TYPE path_append_mem(struct CharBuffer *path, const char *other, size_t other_size)
{
    ARET(!path_absolute_mem(other, other_size));
    PRET(path_append_mem_noroot(path, other, other_size));
    ERROR_RETURN_OK();
}

ERROR_TYPE path_print(struct CharBuffer *path, const char *format, ...)
{
    va_list va;
    ERROR_DECLARE();
    va_start(va, format);
    ERROR_ASSIGN(path_vprint(path, format, va));
    va_end(va);
    ERROR_RETURN();
}

ERROR_TYPE path_vprint(struct CharBuffer *path, const char *format, va_list va)
{
    string_zero(path);
    PRET(path_vprint_append(path, format, va));
    ERROR_RETURN_OK();
}

ERROR_TYPE path_print_append(struct CharBuffer *path, const char *format, ...)
{
    va_list va;
    ERROR_DECLARE();
    va_start(va, format);
    ERROR_ASSIGN(path_vprint_append(path, format, va));
    va_end(va);
    ERROR_RETURN();
}

ERROR_TYPE path_vprint_append(struct CharBuffer *path, const char *format, va_list va)
{
    struct CharBuffer other = ZERO_INIT;
    ERROR_DECLARE();
    #ifdef ERROR_TRACE
        ERROR_ASSIGN(string_internal_vprint_append(&other, false, format, va));
    #else
        ERROR_ASSIGN(string_internal_vprint_append(&other, format, va));
    #endif
    PGOTO(path_append_mem(path, other.p, other.size)); /* Append as unformatted string. Yes, it is inefficient. I don't care. */
    string_finalize(&other);
    ERROR_RETURN_OK();
    
    #ifndef ERROR_DIE
        failure:
        string_finalize(&other);
        ERROR_RETURN();
    #endif
}

bool path_absolute(const struct CharBuffer *path)
{
    /* Path is assumed to be formatted, could have spared some computations */
    return path_absolute_mem(path->p, path->size);
}

bool path_absolute_str(const char *path)
{
    return path_absolute_mem(path, strlen(path));
}

bool path_absolute_mem(const char *path, size_t path_size)
{
    return path_root_size(path, path_size) > 0;
}

ERROR_TYPE path_get_executable_path(struct CharBuffer *path, int argc, char **argv)
{
    #ifdef WIN32
        struct WCharBuffer wpath = ZERO_INIT;
        ERROR_DECLARE();
        (void)argc;
        (void)argv;
        PRET(wchar_buffer_resize(&wpath, 256));
        while (true)
        {
            DWORD size = GetModuleFileNameW(NULL, wpath.p, (DWORD)wpath.size);
            AGOTO(size != 0);
            if (size == wpath.size)
            {
                PGOTO(wchar_buffer_resize(&wpath, 2 * wpath.size));
            }
            else
            {
                wpath.size = size;
                break;
            }
        }
        PGOTO(string_to_string(path, &wpath));
        wchar_buffer_finalize(&wpath);
        ERROR_RETURN_OK();

        #ifndef ERROR_DIE
            failure:
            wchar_buffer_finalize(&wpath);
            ERROR_RETURN();
        #endif
    #else
        ARET(argc > 0);
        ARET(argv[0] != NULL);
        PRET(path_copy_str(path, argv[0]));
        ERROR_RETURN_OK();
    #endif
}

ERROR_TYPE path_get_working_directory(struct CharBuffer *path)
{
    #ifdef WIN32
        struct WCharBuffer wpath = ZERO_INIT;
        DWORD wsize;
        ERROR_DECLARE();
        wsize = GetCurrentDirectoryW(0, NULL);
        PGOTO(wchar_buffer_resize(&wpath, wsize));
        (void)GetCurrentDirectoryW(wsize, wpath.p);
        PGOTO(string_to_string(path, &wpath));
        wchar_buffer_finalize(&wpath);
        ERROR_RETURN_OK();

        #ifndef ERROR_DIE
            failure:
            wchar_buffer_finalize(&wpath);
            ERROR_RETURN();
        #endif
    #else
        PRET(string_resize(path, 256));
        while (true)
        {
            if (getcwd(path->p, path->capacity) == NULL)
            {
                PRET(string_resize(path, 2 * path->size));
            }
            else
            {
                path->size = strlen(path->p);
                break;
            }
        }
        if (path->p[path->size - 1] == '/') { path->size--; path->p[path->size] = '\0'; }
        ERROR_RETURN_OK();
    #endif
}

ERROR_TYPE path_get_directory(struct CharBuffer *directory, const struct CharBuffer *path, bool append_dotdot_if_dotdot)
{
    #ifdef WIN32
    const size_t root_size = path_root_size(path->p, path->size);
    #endif
    size_t size = path->size;
    while (size > 0)
    {
        size--;
        if (path->p[size] == PATH_SEPARATOR)
        {
            /* Reached separator in absolute or relative path */
            #ifdef WIN32
            if (root_size > 0 && size == root_size - 1)
            #else
            if (size == 0)
            #endif
            {
                /* Reached the root of absolute path */
                RET();
            }
            else if (append_dotdot_if_dotdot && path->size - size == 3 && memcmp(path->p + size + 1, "..", 2) == 0)
            {
                /* Trying to step up from directory ending in .. */
                if (directory == path) { /* Do nothing */ }
                else { PRET(string_reserve(directory, path->size + 3)); PIGNORE(string_copy(directory, path)); }
                PRET(string_append_mem(directory, PATH_SEPARATOR_STR "..", 3));
            }
            else
            {
                /* Regular case, just delete directory */
                if (directory == path) { directory->size = size; directory->p[size] = '\0'; }
                else { PRET(string_copy_mem(directory, path->p, size)); }
            }
            ERROR_RETURN_OK();
        }
    }

    /* Relative path with no separators */
    if (append_dotdot_if_dotdot && path->size == 2 && memcmp(path->p, "..", 2) == 0)
    {
        /* Trying to step up from .. */
        if (directory == path) { /* Do nothing */ }
        else { PRET(string_reserve(directory, path->size + 3)); PIGNORE(string_copy(directory, path)); }
        PRET(string_append_mem(directory, PATH_SEPARATOR_STR "..", 3));
    }
    else if (append_dotdot_if_dotdot && path->size == 0)
    {
        /* Trying to step up from "" */
        PRET(string_copy_mem(directory, "..", 2));
    }
    else
    {
        /* True basename */
        string_zero(directory);
    }
    ERROR_RETURN_OK();
}

ERROR_TYPE path_get_basename(struct CharBuffer *basename, const struct CharBuffer *path)
{
    size_t size = path->size;
    while (size > 0)
    {
        size--;
        if (path->p[size] == PATH_SEPARATOR)
        {
            /* Reached separator in absolute or relative path */
            if (basename == path) { PRET(string_replace_mem(basename, 0, size + 1, NULL, 0)); }
            else { PRET(string_copy_mem(basename, path->p + size + 1, path->size - size - 1)); }
            ERROR_RETURN_OK();
        }
    }

    /* Relative path with no separators, aka already basename */
    if (basename == path) { /* Do nothing */ }
    else { PRET(string_copy(basename, path)); }
    ERROR_RETURN_OK();
}
