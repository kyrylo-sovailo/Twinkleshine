#include "../include/path.h"

#include <string.h>

#include <unistd.h>

struct CharBuffer g_application = { 0 };

struct Error *path_set_application(struct CharBuffer *name, const char *argv0)
{
    const char *post_slash;
    post_slash = strrchr(argv0, '/');
    if (post_slash == NULL) post_slash = argv0; /* No slashes */
    else post_slash++;
    PRET(string_copy_str(name, post_slash));
    return OK;
}

struct Error *path_up(struct CharBuffer *path, size_t count)
{
    /* Pop shashes at the end */
    size_t slash = path->size, nonslash;
    while (slash > 0 && path->p[slash - 1] == '/') slash--;
    /* Pop non-shashes at the end */
    nonslash = slash;
    while (nonslash > 0 && path->p[nonslash - 1] != '/') nonslash--;
    
    ARET1(nonslash != 0, "Could not step back from %s", path->p);
    path->size = nonslash;
    path->p[nonslash] = '\0';
    return OK;

    (void)count; /*TODO*/
}

struct Error *path_append_str(struct CharBuffer *path, const char *other, bool is_directory)
{
    const char *other_end = other + strlen(other);
    if (other == other_end) {} /* Other is empty, do nothing */
    else if (other[0] == '/') /* Other is absolute, copy */
    {
        path->size = 0;
        PRET(string_append_mem(path, other, (size_t)(other_end - other)));
    }
    else /* Relative append */
    {
        while (other < other_end)
        {
            const char *slash = strchr(other, '/');
            if (slash == NULL) slash = other_end - 1;
            if (other == slash) {}
            else if (other + 1 == slash && other[0] == '.') {}
            else if (other + 2 == slash && other[0] == '.' && other[1] == '.') { PRET(path_up(path, 1)); break; }
            else { PRET(string_append_mem(path, other, (size_t)(slash - other + 1))); break; }
            other = slash + 1;
        }
    }
    if (is_directory && (path->size == 0 || path->p[path->size - 1] != '/')) /* Append slash */
    {
        PRET(string_push(path, '/'));
    }
    else if (!is_directory && path->size > 0 && path->p[path->size - 1] == '/') /* Pop slash */
    {
        path->size--;
        path->p[path->size - 1] = '\0';
    }
    return OK;
}
