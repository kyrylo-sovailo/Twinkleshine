#ifndef COMMONLIB_BOOL_H
#define COMMONLIB_BOOL_H

#if defined(__cplusplus)
    /* bool is built-in in C++ -> do nothing */
#elif defined(bool)
    /* bool was already defined as a macro (MSVC is known to do that) -> do nothing */
#elif defined(LSP)
    /* clangd assumes built-in bool and can't handle typedef -> include stdbool.h */
    #include <stdbool.h>
#elif defined(_MSC_VER)
    /* MSVC defines bool implicitly and can't handle typedef -> include stdbool.h */
    #include <stdbool.h>
#elif __STDC_VERSION__ >= 199901L
    /* C after C99 has stdbool.h -> include stdbool */
    #include <stdbool.h>
#else
    /* General case -> define custom bool */
    typedef enum { false = 0, true = 1 } bool;
#endif

#endif
