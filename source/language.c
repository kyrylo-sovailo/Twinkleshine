#include "../include/language.h"

static bool language_explicit(char language, char explicit_request, char implicit_request)
{
    if (implicit_request == '\0') implicit_request = IMPLICIT_LANGUAGE;
    if (language != implicit_request) return true; /* We absolutely need to specify */
    if (explicit_request != '\0' && explicit_request == implicit_request) return true; /* Specified default explicitly? You really want to have it then */
    return false; /* Nah */
}

char language_get(const char language[MAX_LANGUAGE_SIZE], unsigned char language_size)
{
    if (language_size == 2 && language[0] == 'e' && language[1] == 'n') return 'e';
    if (language_size == 2 && language[0] == 'd' && language[1] == 'e') return 'd';
    return '\0';
}

const char *language_question(char language, char explicit_request, char implicit_request)
{
    const bool explicit = language_explicit(language, explicit_request, implicit_request);
    if (language == 'e') return explicit ? "?lang=en" : "";
    if (language == 'd') return explicit ? "?lang=de" : "";
    return "";
}

const char *language_slash(char language, char explicit_request, char implicit_request)
{
    const bool explicit = language_explicit(language, explicit_request, implicit_request);
    if (language == 'e') return explicit ? "/en" : "";
    if (language == 'd') return explicit ? "/de" : "";
    return "";
}
