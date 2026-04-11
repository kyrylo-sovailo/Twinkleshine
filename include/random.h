#ifndef RANDOM_H
#define RANDOM_H

#include <stddef.h>

/* rand() but size_t and accurate */
size_t fair_rand(size_t outcome_number);

#endif
