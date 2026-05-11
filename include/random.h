#ifndef RANDOM_H
#define RANDOM_H

#include <stddef.h>

/* Initializes the module */
void random_module_initialize(void);

/* rand() but size_t and accurate */
size_t random_rand(size_t outcome_number);

#endif
