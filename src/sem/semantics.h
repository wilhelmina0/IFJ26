/**
 * @file semantics.h
 * @brief Semantic analysis interface
 *
 * Checks the AST and stores types, symbol identifiers and conversions
 * needed by the code generator.
 *
 * @authors
 * - Radim Winter
 *
 */
#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "error.h"

typedef struct Node Node;

/** Details of the first error found during semantic analysis. */
typedef struct {
    ErrorCode code;
    int line;
    const char *message; /**< Static message; the caller must not free it. */
} SemanticError;

/**
 * @brief Checks function signatures, then checks their bodies.
 *
 * Adds types, symbol identifiers and Int-to-Double conversion flags to the AST.
 * Temporary symbol tables are freed before returning. The caller owns the AST.
 * Only use the annotated AST for generation after SUCCESS; on failure some nodes
 * may already be annotated. This function does not call the generator.
 * Internal errors are printed by handleError and terminate the compiler with code 99.
 *
 * @param program Complete AST_PROGRAM root with the layout in ast.h.
 * @param err Optional error output; may be NULL.
 * @return SUCCESS or the first semantic/syntax error.
 */
ErrorCode semanticsRun(Node *program, SemanticError *err);

#endif // SEMANTICS_H
