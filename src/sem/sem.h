/**
 * @file sem.h
 * @brief semantic analysis interface
 *
 * @authors
 * - Radim Winter
 */
#ifndef SEM_H
#define SEM_H

#include "error.h"

typedef struct Node Node;

// semantic error code, source line and message
typedef struct {
    ErrorCode code;
    int line;
    const char *message; // static text, no allocation to free
} SemanticError;

ErrorCode semanticsRun(Node *program, SemanticError *err);

#endif // SEM_H
