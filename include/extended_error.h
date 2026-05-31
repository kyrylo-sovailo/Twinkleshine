#ifndef EXTENDED_ERROR_H
#define EXTENDED_ERROR_H

#include "../commonlib/include/bool.h"
#include "../commonlib/include/char.h"
#include "../commonlib/include/macro.h"

struct Error;

/* Action that should be taken in error handling */
enum ExErrorFlag
{
    /* Formula: [SEND] & [SHUTDOWN|CLOSE] & [LOG] & [DIE]  */
    PARTIAL_EEF_SEND        = 1,    /* Send fixed message to the client */
    PARTIAL_EEF_SHUTDOWN    = 2,    /* Start the shutdown process */
    PARTIAL_EEF_CLOSE       = 4,    /* Close the connection immediately */
    PARTIAL_EEF_LOG         = 8,    /* Log the incident */
    PARTIAL_EEF_DIE         = 16    /* Total failure */
};

/*
Note:
It is extremely important to avoid recursion during TLS shutdown 
EEF_SHUTDOWN may only occur in several places:
- during timeout check
- during parsing
First one is avoided because of the 'first' flag
Second one is avoided parsing does not happen when in pending shutdown mode
*/

#define EEF_SHUTDOWN_LOG PARTIAL_EEF_SHUTDOWN | PARTIAL_EEF_LOG
#define EEF_SEND_SHUTDOWN_LOG(FIXED_RESPONSE) PARTIAL_EEF_SEND | PARTIAL_EEF_SHUTDOWN | PARTIAL_EEF_LOG | FIXED_RESPONSE

#define EEF_CLOSE_LOG PARTIAL_EEF_CLOSE | PARTIAL_EEF_LOG
#define EEF_CLOSE_LOG_DIE PARTIAL_EEF_CLOSE | PARTIAL_EEF_LOG | PARTIAL_EEF_DIE
#define EEF_SEND_CLOSE_LOG(FIXED_RESPONSE) PARTIAL_EEF_SEND | PARTIAL_EEF_CLOSE | PARTIAL_EEF_LOG | FIXED_RESPONSE

struct ExError { struct Error *error; enum ExErrorFlag flags; };

struct ExError exerror_internal_allocate(enum ExErrorFlag flags, const cchar_t *format, ...) NODISCARD PRINTFLIKE(2, 3);
struct ExError exerror_internal_allocate_append(struct ExError exerror, const cchar_t *format, ...) NODISCARD PRINTFLIKE(2, 3);
struct ExError exerror_internal_allocate_append_extend(struct Error *error, enum ExErrorFlag flags, const cchar_t *format, ...) NODISCARD PRINTFLIKE(3, 4);

#define EXRET(FLAGS) { return exerror_internal_allocate(FLAGS, ERROR_FORMAT()); }
#define EXRET0(FORMAT, FLAGS) { return exerror_internal_allocate(FLAGS, ERROR_FORMAT_F(FORMAT)); }
#define EXRET1(FORMAT, A, FLAGS) { return exerror_internal_allocate(FLAGS, ERROR_FORMAT_F(FORMAT), A); }
#define EXAGOTO(EXPRESSION, FLAGS) { const bool check = EXPRESSION; if (check) {} else { exerror = exerror_internal_allocate(FLAGS, ERROR_FORMAT_E(#EXPRESSION)); goto failure; } }
#define EXAGOTO0(EXPRESSION, FORMAT, FLAGS) { const bool check = EXPRESSION; if (check) {} else { exerror = exerror_internal_allocate(FLAGS, ERROR_FORMAT_EF(#EXPRESSION, FORMAT)); goto failure; } }
#define EXARET(EXPRESSION, FLAGS) { const bool check = EXPRESSION; if (check) {} else return exerror_internal_allocate(FLAGS, ERROR_FORMAT_E(#EXPRESSION)); }
#define EXARET0(EXPRESSION, FORMAT, FLAGS) { const bool check = EXPRESSION; if (check) {} else return exerror_internal_allocate(FLAGS, ERROR_FORMAT_EF(#EXPRESSION, FORMAT)); }
#define EXPGOTO(EXPRESSION) { struct ExError check = EXPRESSION; if (check.error != OK) { exerror = exerror_internal_allocate_append(check, ERROR_FORMAT_E(#EXPRESSION)); goto failure; } }
#define EXPGOTOF(EXPRESSION, FLAGS) { struct Error *check = EXPRESSION; if (check != OK) { exerror = exerror_internal_allocate_append_extend(check, FLAGS, ERROR_FORMAT_E(#EXPRESSION)); goto failure; } }
#define EXPRET(EXPRESSION) { struct ExError check = EXPRESSION; if (check.error != OK) return exerror_internal_allocate_append(check, ERROR_FORMAT_E(#EXPRESSION)); }
#define EXPRETF(EXPRESSION, FLAGS) { struct Error *check = EXPRESSION; if (check != OK) return exerror_internal_allocate_append_extend(check, FLAGS, ERROR_FORMAT_E(#EXPRESSION)); }
#define EXPIGNORE(EXPRESSION) { struct ExError check = EXPRESSION; if (check.error != OK) {} else {} }

#endif
