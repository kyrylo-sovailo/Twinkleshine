#ifndef UTILITY_H
#define UTILITY_H

#include <stddef.h>

size_t int_length(size_t a);

const char *string_errno(void);
const char *string_ssl_error(int error);
const char *string_ssl_reason(void);

#endif
