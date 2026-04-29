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
#ifdef COMMON_WCHAR
    #include <wchar.h>
#endif

struct CharBuffer g_application = ZERO_INIT;
struct CharBuffer g_directory = ZERO_INIT;
struct CharBuffer g_working_directory = ZERO_INIT;

static const cchar_t *path_next_separator(const cchar_t *p, size_t size)
{
    #ifdef WIN32
        const cchar_t *found = NULL;
        const cchar_t *copy = p;
        for (copy = p; copy < p + size; copy++)
        {
            if (*copy == COMMON_L('/') || *copy == COMMON_L('\\')) { found = copy; break; }
        }
        return found;
    #else
        return (const cchar_t*)COMMON_W(memchr(p, COMMON_L('/'), size));
    #endif
}

#ifdef WIN32

static size_t path_root_size(const cchar_t *p, size_t size)
{
    /* Path like C:\ */
    if (size >= 3
    && ((p[0] >= COMMON_L('a') && p[0] <= COMMON_L('z')) || (p[0] >= COMMON_L('A') && p[0] <= COMMON_L('Z')))
    && (p[1] == COMMON_L(':'))
    && (p[2] == COMMON_L('/') || p[2] == COMMON_L('\\')))
    {
        return 3;
    }

    /* Path like \\?\C:\ */
    else if (size >= 7
    && (p[0] == COMMON_L('/') || p[0] == COMMON_L('\\'))
    && (p[1] == COMMON_L('/') || p[1] == COMMON_L('\\'))
    && (p[2] == '?')
    && (p[3] == COMMON_L('/') || p[3] == COMMON_L('\\'))
    && ((p[4] >= COMMON_L('a') && p[4] <= COMMON_L('z')) || (p[4] >= COMMON_L('A') && p[4] <= COMMON_L('Z')))
    && (p[5] == COMMON_L(':'))
    && (p[6] == COMMON_L('/') || p[6] == COMMON_L('\\')))
    {
        return 7;
    }

    /*
    The list of Windows path types:
    1. Relative path on current disk:       folder\file.txt (implemented)
    2. Relative path on other disk:         C:folder\file.txt
    3. Absolute path on current disk:       \folder\file.txt
    4. Absolute path on other disk:         C:\folder\file.txt (implemented)
    5. Long absolute path on other disk:    \\?\C:\folder\file.txt (implemented)
    6. Pipe name:                           \\PipeName\
    7. Long pipe name:                      \\?\UNC\PipeName\
    8. Internal device name:                \\.\DeviceName
    9. Internal file name:                  \??\C:\folder\file.txt
    */
    return 0;
}

static ERROR_TYPE path_prepand_remove_long_root(struct CharBuffer *path, size_t root_size) NODISCARD;
static ERROR_TYPE path_prepand_remove_long_root(struct CharBuffer *path, size_t root_size)
{
    if (root_size == 0) { /* Do nothing */ }
    else if (root_size == 3 && path->size + 1 > MAX_PATH) { PRET(string_insert_mem(path, 0, COMMON_L("\\\\?\\"), 4)); }
    else if (root_size == 7 && path->size + 1 <= MAX_PATH + 4) { PRET(string_remove(path, 0, 4)); }
    ERROR_RETURN_OK();
}

#else

static size_t path_root_size(const cchar_t *p, size_t size)
{
    if (size >= 1 && p[0] == COMMON_L('/')) return 1;
    return 0;
}

#endif

static ERROR_TYPE path_append_mem_noroot(struct CharBuffer *path, const cchar_t *other, size_t other_size) NODISCARD;
static ERROR_TYPE path_append_mem_noroot(struct CharBuffer *path, const cchar_t *other, size_t other_size)
{
    const cchar_t *p = other;
    size_t size = other_size;
    while (true)
    {
        const cchar_t *found = path_next_separator(p, size);
        const size_t part_size = (found == NULL) ? size : (size_t)(found - p);
        if (part_size == 0)
        {
            /* Double slash, do nothing */
        }
        else if (part_size == 1 && COMMON_W(memcmp(p, COMMON_L("."), 1)) == 0)
        {
            /* Dot, do nothing */
        }
        else if (part_size == 2 && COMMON_W(memcmp(p, COMMON_L(".."), 2)) == 0)
        {
            /* Double dot, step back */
            PRET(path_get_directory(path, path, true));
        }
        else
        {
            /* Regular case */
            const cchar_t *nonvalid_found;
            for (nonvalid_found = p; nonvalid_found < p + part_size; nonvalid_found++)
            {
                ARET((unsigned int)*nonvalid_found >= (unsigned int)' ');
                #ifdef WIN32
                    ARET(COMMON_W(memchr(COMMON_L("<>:\"|?*"), *nonvalid_found, 7)) == NULL);
                #endif
            }
            if (path->size > 0 && path->p[path->size - 1] != COMMON_SEPARATOR) PRET(string_push(path, COMMON_SEPARATOR)); /* Protection against root */
            PRET(string_append_mem(path, p, part_size));
        }
        if (found == NULL) break;
        p = found + 1;
        size = size - part_size - 1;
    }
    ERROR_RETURN_OK();
}

ERROR_TYPE path_module_initialize(int argc, cchar_t **argv)
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

ERROR_TYPE path_copy_str(struct CharBuffer *path, const cchar_t *other)
{
    PRET(path_copy_mem(path, other, COMMON_WCS(len(other))));
    ERROR_RETURN_OK();
}

ERROR_TYPE path_copy_mem(struct CharBuffer *path, const cchar_t *other, size_t other_size)
{
    const size_t root_size = path_root_size(other, other_size);
    #ifdef WIN32
        size_t i;
        PRET(string_resize(path, root_size));
        for (i = 0; i < root_size; i++) path->p[i] = (other[i] == COMMON_L('/')) ? COMMON_L('\\') : other[i];
        PRET(path_append_mem_noroot(path, other + root_size, other_size - root_size));
        PRET(path_prepand_remove_long_root(path, root_size));
    #else
        PRET(string_copy_mem(path, other, root_size));
        PRET(path_append_mem_noroot(path, other + root_size, other_size - root_size));
    #endif
    ERROR_RETURN_OK();
}

ERROR_TYPE path_append(struct CharBuffer *path, const struct CharBuffer *other)
{
    /* Special form, assuming other is formatted */
    const cchar_t *p = other->p;
    size_t size = other->size;
    ARET(!path_absolute(other));
    while (true)
    {
        const cchar_t *found = path_next_separator(p, size);
        const size_t part_size = (found == NULL) ? size : (size_t)(found - p);
        if (part_size == 2 && COMMON_W(memcmp(p, COMMON_L(".."), 2)) == 0)
        {
            /* Double dot, step back */
            PRET(path_get_directory(path, path, true));
        }
        else
        {
            /* Regular case, append everything and quit */
            if (path->size > 0) PRET(string_push(path, COMMON_SEPARATOR));
            PRET(string_append_mem(path, p, size));
            break;
        }
        if (found == NULL) break;
        p = found + 1;
        size = size - part_size - 1;
    }
    #ifdef WIN32
        PRET(path_prepand_remove_long_root(path, path_root_size(path->p, path->size)));
    #endif
    ERROR_RETURN_OK();
}

ERROR_TYPE path_append_str(struct CharBuffer *path, const cchar_t *other)
{
    PRET(path_append_mem(path, other, COMMON_WCS(len(other))));
    ERROR_RETURN_OK();
}

ERROR_TYPE path_append_mem(struct CharBuffer *path, const cchar_t *other, size_t other_size)
{
    ARET(!path_absolute_mem(other, other_size));
    PRET(path_append_mem_noroot(path, other, other_size));
    #ifdef WIN32
        PRET(path_prepand_remove_long_root(path, path_root_size(path->p, path->size)));
    #endif
    ERROR_RETURN_OK();
}

ERROR_TYPE path_print(struct CharBuffer *path, const cchar_t *format, ...)
{
    va_list va;
    ERROR_DECLARE();
    va_start(va, format);
    ERROR_ASSIGN(path_vprint(path, format, va));
    va_end(va);
    ERROR_RETURN();
}

ERROR_TYPE path_vprint(struct CharBuffer *path, const cchar_t *format, va_list va)
{
    string_zero(path);
    PRET(path_vprint_append(path, format, va));
    ERROR_RETURN_OK();
}

ERROR_TYPE path_print_append(struct CharBuffer *path, const cchar_t *format, ...)
{
    va_list va;
    ERROR_DECLARE();
    va_start(va, format);
    ERROR_ASSIGN(path_vprint_append(path, format, va));
    va_end(va);
    ERROR_RETURN();
}

ERROR_TYPE path_vprint_append(struct CharBuffer *path, const cchar_t *format, va_list va)
{
    struct CharBuffer other = ZERO_INIT;
    ERROR_DECLARE();
    ERROR_ASSIGN(string_internal_vprint_append(&other, false, format, va));
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

bool path_absolute_str(const cchar_t *path)
{
    return path_absolute_mem(path, COMMON_WCS(len(path)));
}

bool path_absolute_mem(const cchar_t *path, size_t path_size)
{
    return path_root_size(path, path_size) > 0;
}

ERROR_TYPE path_get_executable_path(struct CharBuffer *path, int argc, cchar_t **argv)
{
    #ifdef WIN32
        (void)argc;
        (void)argv;
        PRET(char_buffer_resize(path, 256));
        while (true)
        {
            DWORD size = GetModuleFileName(NULL, path->p, (DWORD)path->size);
            ARET(size > 0);
            if (size == path->size)
            {
                PRET(char_buffer_resize(path, 2 * path->size));
            }
            else
            {
                path->size = size;
                break;
            }
        }
        PRET(path_prepand_remove_long_root(path, path_root_size(path->p, path->size)));
    #else
        ARET(argc > 0);
        ARET(argv[0] != NULL);
        PRET(path_copy_str(path, argv[0]));
    #endif
    ERROR_RETURN_OK();
}

ERROR_TYPE path_get_working_directory(struct CharBuffer *path)
{
    #ifdef WIN32
        size_t size = GetCurrentDirectoryW(0, NULL);
        PRET(string_resize(path, size - 1));
        (void)GetCurrentDirectory((DWORD)size, path->p);
        PRET(path_prepand_remove_long_root(path, path_root_size(path->p, path->size)));
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
                path->size = COMMON_WCS(len(path->p));
                break;
            }
        }
        if (path->p[path->size - 1] == COMMON_L('/')) { path->size--; path->p[path->size] = COMMON_L('\0'); }
    #endif
    ERROR_RETURN_OK();
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
        if (path->p[size] != COMMON_SEPARATOR) continue;

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
        else if (append_dotdot_if_dotdot && path->size - size == 3 && COMMON_W(memcmp(path->p + size + 1, COMMON_L(".."), 2)) == 0)
        {
            /* Trying to step up from directory ending in .. */
            if (directory == path) { /* Do nothing */ }
            else { PRET(string_reserve(directory, path->size + 3)); PIGNORE(string_copy(directory, path)); }
            PRET(string_append_mem(directory, COMMON_SEPARATOR_STR COMMON_L(".."), 3));
        }
        else
        {
            /* Regular case, just delete directory */
            if (directory == path) { directory->size = size; directory->p[size] = COMMON_L('\0'); }
            else { PRET(string_copy_mem(directory, path->p, size)); }
        }
        #ifdef WIN32
        PRET(path_prepand_remove_long_root(directory, root_size));
        #endif
        ERROR_RETURN_OK();
    }

    /* Relative path with no separators */
    if (append_dotdot_if_dotdot && path->size == 2 && COMMON_W(memcmp(path->p, COMMON_L(".."), 2)) == 0)
    {
        /* Trying to step up from .. */
        if (directory == path) { /* Do nothing */ }
        else { PRET(string_reserve(directory, path->size + 3)); PIGNORE(string_copy(directory, path)); }
        PRET(string_append_mem(directory, COMMON_SEPARATOR_STR COMMON_L(".."), 3));
    }
    else if (append_dotdot_if_dotdot && path->size == 0)
    {
        /* Trying to step up from "" */
        PRET(string_copy_mem(directory, COMMON_L(".."), 2));
    }
    else
    {
        /* True basename */
        string_zero(directory);
    }
    /* no path_prepand_remove_long_root() because path is relative */
    ERROR_RETURN_OK();
}

ERROR_TYPE path_get_basename(struct CharBuffer *basename, const struct CharBuffer *path)
{
    size_t size = path->size;
    while (size > 0)
    {
        size--;
        if (path->p[size] != COMMON_SEPARATOR) continue;
        
        /* Reached separator in absolute or relative path */
        if (basename == path) { PRET(string_replace_mem(basename, 0, size + 1, NULL, 0)); }
        else { PRET(string_copy_mem(basename, path->p + size + 1, path->size - size - 1)); }
        ERROR_RETURN_OK();
    }

    /* Relative path with no separators, aka already basename */
    if (basename == path) { /* Do nothing */ }
    else { PRET(string_copy(basename, path)); }
    ERROR_RETURN_OK();
}
