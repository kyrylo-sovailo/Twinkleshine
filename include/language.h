#ifndef LANGUAGE_H
#define LANGUAGE_H

#include "../commonlib/include/bool.h"
#include "value.h"

#define MAX_LANGUAGE_SIZE 2
#define IMPLICIT_LANGUAGE 'e'

char language_get(const char language[MAX_LANGUAGE_SIZE], unsigned char language_size);
const char *language_question(char language, char explicit_request, char implicit_request);
const char *language_slash(char language, char explicit_request, char implicit_request);

#endif
