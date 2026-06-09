#include "../include/utility.h"

size_t int_length(size_t a)
{
    size_t length = 1;
    size_t threshold = 10;
    while (a >= threshold)
    {
        threshold *= 10;
        length++;
    }
    return length;
}
