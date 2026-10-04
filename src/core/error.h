/**
 * @file error.h
 * @brief Shared error handling
 *
 * Defines input and internal compiler errors
 * used across compiler mudules.
 * @authors
 * - Bára Wilhelmová(xwilheb00)
 *
 */

#ifndef ERROR_H
#define ERROR_H

typedef enum {
    SUCCESS,
    LEX_ERROR,
    SYN_ERROR,
    SEM_ERROR_UNDEFINED,  // undefined function, var or type
    SEM_ERROR_REDEFINED,  // unallowed redefinition
    SEM_ERROR_INVALID,    // wrong count/type args or params
    SEM_ERROR_NOMATCH,    // type incompability
    SEM_ERROR_VAL,        // unmodifiable var/param
    SEM_ERROR_OTHER = 10,
    INTERNAL_ERROR = 99   // error caused by compiler NOT input
} ErrorCode;

typedef struct {
    ErrorCode code;
    const char *message;
} Error;

#endif //ERROR_H
