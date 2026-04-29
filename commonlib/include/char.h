#ifndef COMMONLIB_CHAR_H
#define COMMONLIB_CHAR_H

/*
The library takes native approach and uses cchars unless explicitly specified.
NCHAR = narrow char         (exists everywhere)
WCHAR = wide character      (exists only on Windows)
CCHAR = common character    (exists everywhere, defined differently)
 CHAR = common character    (exists everywhere, defined differently)
*/

#if defined(WIN32) && defined(UNICODE)
    #include <stddef.h>     /* Wide   */
    typedef char nchar_t;   /* Narrow */
    typedef wchar_t cchar_t;/* Common */
    #define COMMON_WCHAR
    #define COMMON_L_INTERNAL(TEXT) L##TEXT
    #define COMMON_L(TEXT) COMMON_L_INTERNAL(TEXT)
    #define COMMON_BINARY(WCHAR_PREFIX, FUNCTION) WCHAR_PREFIX##FUNCTION
    #define COMMON_TERNARY(NCHAR_PREFIX, WCHAR_PREFIX, FUNCTION) WCHAR_PREFIX##FUNCTION
    #define COMMON_LONG L"l"
#else
                            /* Wide   */
    typedef char nchar_t;   /* Narrow */
    typedef char cchar_t;   /* Common */
    #define COMMON_L(TEXT) TEXT
    #define COMMON_BINARY(WCHAR_PREFIX, FUNCTION) FUNCTION
    #define COMMON_TERNARY(NCHAR_PREFIX, WCHAR_PREFIX, FUNCTION) NCHAR_PREFIX##FUNCTION
    #define COMMON_LONG
#endif

/* Function names */
#define COMMON_W(FUNCTION) COMMON_BINARY(w, FUNCTION)
#define COMMON__W(FUNCTION) COMMON_BINARY(_w, FUNCTION)
#define COMMON_WCS(FUNCTION) COMMON_TERNARY(str, wcs, FUNCTION)
#define COMMON_SW(FUNCTION) COMMON_TERNARY(s, sw, FUNCTION)
#define COMMON_VFW(FUNCTION) COMMON_TERNARY(vf, vfw, FUNCTION)

/* Sequences */
#define COMMON_E COMMON_L("")

#define COMMON_C COMMON_L("%") COMMON_LONG COMMON_L("c")        /* %c or %lc */
#define COMMON_WC COMMON_L("%*") COMMON_LONG COMMON_L("c")      /* %*c or %*lc */
#define COMMON_PC COMMON_L("%.*") COMMON_LONG COMMON_L("c")     /* %.*c or %.*lc */
#define COMMON_WPC COMMON_L("%*.*") COMMON_LONG COMMON_L("c")   /* %*.*c or %*.*lc */
#define COMMON_QC COMMON_L("'%") COMMON_LONG COMMON_L("c'")     /* '%c' or '%lc' */
#define COMMON_QQC COMMON_L("\"%") COMMON_LONG COMMON_L("c\"")  /* "%c" or "%lc" */

#define COMMON_S COMMON_L("%") COMMON_LONG COMMON_L("s")        /* %s or %ls */
#define COMMON_WS COMMON_L("%*") COMMON_LONG COMMON_L("s")      /* %*s or %*ls */
#define COMMON_PS COMMON_L("%.*") COMMON_LONG COMMON_L("s")     /* %.*s or %.*ls */
#define COMMON_WPS COMMON_L("%*.*") COMMON_LONG COMMON_L("s")   /* %*.*s or %*.*ls */
#define COMMON_QS COMMON_L("'%") COMMON_LONG COMMON_L("s'")     /* '%s' or '%ls' */
#define COMMON_QQS COMMON_L("\"%") COMMON_LONG COMMON_L("s\"")  /* "%s" or "%ls" */

#define COMMON_S2(STRING1, STRING2) COMMON_L(STRING1) COMMON_S COMMON_L(STRING2)
#define COMMON_S3(STRING1, STRING2, STRING3) COMMON_L(STRING1) COMMON_S COMMON_L(STRING2) COMMON_S COMMON_L(STRING3)

/* OS-dependent */
#if defined(WIN32)
    #define COMMON_SEPARATOR COMMON_L('\\')
    #define COMMON_SEPARATOR_STR COMMON_L("\\")
    #define COMMON_N COMMON_L("\r\n")
#else
    #define COMMON_SEPARATOR COMMON_L('/')
    #define COMMON_SEPARATOR_STR COMMON_L("/")
    #define COMMON_N COMMON_L("\n")
#endif

#endif
