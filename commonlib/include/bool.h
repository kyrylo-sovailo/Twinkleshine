#ifndef COMMONLIB_BOOL_H
#define COMMONLIB_BOOL_H

#if defined(__cplusplus)
    /* Do nothing */
#elif defined(LSP)
    /* Special case because clangd assumes built-in bool and can't handle typedef enum */
    #include <stdbool.h>
#elif __STDC_VERSION__ < 199901L
    /* Before C99 */
    typedef enum { false = 0, true = 1 } bool;
#else
    /* After C99 */
    #include <stdbool.h>
#endif

#endif
