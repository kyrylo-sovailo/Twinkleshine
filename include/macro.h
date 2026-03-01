#ifndef MACRO_H
#define MACRO_H

/* Function attributes */
#ifdef __GNUC__
    #define PRINTFLIKE(FORMAT_POSITION, DOTS_POSITION) __attribute__((format(printf, FORMAT_POSITION, DOTS_POSITION)))
    #define NODISCARD __attribute__((warn_unused_result))
    #define NORETURN __attribute__((noreturn))
#else
    #define PRINTFLIKE(FORMAT_POSITION, DOTS_POSITION)
    #define NODISCARD
    #define NORETURN
#endif

/* Static analysis fixers */
#ifdef __GNUC__
    #ifndef __clang__
        #define ZERO_AND_FORGET(POINTER) \
            _Pragma("GCC diagnostic push") \
            _Pragma("GCC diagnostic ignored \"-Wanalyzer-malloc-leak\"") \
            memset(POINTER, 0, sizeof(*POINTER)); \
            _Pragma("GCC diagnostic pop")
    #endif
#endif
#ifndef ZERO_AND_FORGET
    #define ZERO_AND_FORGET(POINTER)
#endif

/* Stringification */
#define STRINGIZE(A) STRINGIZE_INTERNAL(A)
#define STRINGIZE_INTERNAL(A) #A

/* File and line */
#ifndef __RELATIVE_FILE__
    #define __RELATIVE_FILE__ __FILE__
#endif
#ifndef __BASENAME_FILE__
    #define __BASENAME_FILE__ __FILE__
#endif
#define __STRING_LINE__ STRINGIZE(__LINE__)

#endif
