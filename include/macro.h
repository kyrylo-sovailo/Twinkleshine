#ifndef MACRO_H
#define MACRO_H

/* Static analysis fixers */
#if defined(__GNUC__) && !defined(__clang__)
    #define ZERO_AND_FORGET(TYPE, VARIABLE) \
        _Pragma("GCC diagnostic push") \
        _Pragma("GCC diagnostic ignored \"-Wanalyzer-malloc-leak\"") \
        { const TYPE zero = ZERO_INIT; VARIABLE = zero; } \
        _Pragma("GCC diagnostic pop")
#else
    #define ZERO_AND_FORGET(VARIABLE, VALUE)
#endif

#endif
