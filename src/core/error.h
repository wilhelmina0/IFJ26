/**
 * @file error.h
 * @brief Shared error handling
 *
 * Defines input and internal compiler errors used across compiler modules.
 *
 * @authors
 * - Bára Wilhelmová (xwilheb00)
 *
 */

#ifndef ERROR_H
#define ERROR_H
#include <stdio.h>
#include <stdlib.h>

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
    int line;
} Error;

static inline void printError(Error *err) {
    fprintf(stderr, "Error %d (line: %d): %s\n",(int)err->code, err->line, err->message);
}

// inline so there is no need for error.c
static inline void handleError(Error *err, ErrorCode code, const char *message, int line) {
    Error error;
    error.code = code;
    error.message = message;
    error.line = line;

    if (code == INTERNAL_ERROR) {
        printError(&error);
        exit((int)code);
    }

    *err = error;
}

#endif //ERROR_H
