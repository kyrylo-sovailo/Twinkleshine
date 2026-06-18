#include "../include/language.h"

char language_get(const char language[MAX_LANGUAGE_SIZE], unsigned char language_size)
{
    if (language_size == 2 && language[0] == 'e' && language[1] == 'n') return 'e';
    if (language_size == 2 && language[0] == 'd' && language[1] == 'e') return 'd';
    return '?';
}

const char *language_question(char language)
{
    if (language == 'e') return "?lang=en";
    if (language == 'd') return "?lang=de";
    return "";
}

const char *language_slash(char language)
{
    if (language == 'e') return "/en";
    if (language == 'd') return "/de";
    return "";
}
