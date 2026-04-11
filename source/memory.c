#include "../include/memory.h"

#include <stdlib.h>

size_t g_memory;

void *count_malloc(size_t new_size)
{
    void *new_p = malloc(new_size);
    if (new_p != NULL) g_memory += new_size;
    return new_p;
}

void *count_realloc(void *old_memory, size_t old_size, size_t new_size)
{
    void *new_p = realloc(old_memory, new_size);
    if (new_p != NULL) g_memory += (new_size - old_size);
    return new_p;
}

void count_free(void *old_memory, size_t old_size)
{
    g_memory -= old_size;
    free(old_memory);
}
