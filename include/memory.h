#ifndef MEMORY_H
#define MEMORY_H

#include <stddef.h>

extern size_t g_memory;

void *count_malloc(size_t new_size);
void *count_realloc(void *old_memory, size_t old_size, size_t new_size);
void count_free(void *old_memory, size_t old_size);

#endif
