#ifndef BOOL_H
#define BOOL_H

#ifdef LSP /* Override because clangd assumes built-in bool and can't handle typedef enum */

#include <stdbool.h>

#else /* General case */

#if __STDC_VERSION__ < 199901L
    typedef enum { false = 0, true = 1 } bool;
#else
    #include <stdbool.h>
#endif

#endif

#endif
