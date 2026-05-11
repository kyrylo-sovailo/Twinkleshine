#include "../include/random.h"
#include "../commonlib/include/bool.h"

#include <stdlib.h>
#include <time.h>

void random_module_initialize(void)
{
    const time_t t = time(NULL);
    const clock_t c = clock();
    const unsigned int seed = (unsigned int)t + (unsigned int)c;
    srand(seed);
}

size_t random_rand(size_t outcome_number)
{
    size_t max_buffer, reject_number, max_buffer_accept;
    unsigned char rand_max_bits, max_bits, rand_bits;
    
    /* Verify that RAND_MAX + 1 is power of 2 */
    bool check[((((size_t)RAND_MAX + 1) & (size_t)RAND_MAX) == 0) ? 1 : -1];
    (void)check;

    if (outcome_number == 0) return 0;
    /* Get size of RAND_MAX in bits */
    for (max_buffer = RAND_MAX, rand_max_bits = 0; max_buffer > 0; max_buffer /= 2, rand_max_bits++) {}
    /* Get size of outcome_number - 1 in bits */
    for (max_buffer = outcome_number - 1, max_bits = 0; max_buffer > 0; max_buffer /= 2, max_bits++) {}

    /* Get maximum possible buffer value */
    for (max_buffer = 0, rand_bits = 0; rand_bits < max_bits; rand_bits += max_bits) max_buffer = (max_buffer << rand_max_bits) | RAND_MAX;
    /* Compute range of acceptable numbers */
    reject_number = (max_buffer % outcome_number + 1 == outcome_number) ? 0 : (max_buffer % outcome_number + 1); /* reject = (max_buffer + 1) % outcome_number; */
    max_buffer_accept = max_buffer - reject_number;

    /* Sample */
    while (true)
    {
        size_t buffer;
        for (buffer = 0, rand_bits = 0; rand_bits < max_bits; rand_bits += max_bits) buffer = (buffer << rand_max_bits) | (size_t)rand();
        if (buffer <= max_buffer_accept) return buffer % outcome_number;
    }
}