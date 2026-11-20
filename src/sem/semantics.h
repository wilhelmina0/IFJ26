/**
 * @file semantics.h
 * @brief Semantic analysis
 *
 * Checks the AST in two passes and stores semantic information
 * needed by the code generator.
 *
 * @authors
 * - Radim Winter (xwinter01)
 *
 */

#ifndef SEMANTICS_H
#define SEMANTICS_H

#include "ast.h"
#include "error.h"
#include "symtable.h"

typedef struct {
    ErrorCode code;
    int line;
    const char *message; // Static message, not owned by this structure.
} SemanticError;

/**
 * @brief Performs semantic analysis of the complete AST.
 *
 * Registers function signatures, then checks function bodies.
 * Stores types, symbol identifiers and conversions in the AST.
 * The caller keeps ownership of the AST; temporary tables are released.
 *
 * @param program Root AST_PROGRAM node
 * @param err Optional output error; may be NULL
 * @return SUCCESS on success, otherwise the first error code
 */
ErrorCode semanticsCheck(Node *program, SemanticError *err);

/**
 * @brief Checks the AST and passes it to the code generator.
 *
 * Calls generateCode only after successful semantic analysis.
 * The caller keeps ownership of the AST even if an error occurs.
 *
 * @param program Root AST_PROGRAM node
 * @param err Optional output error; may be NULL
 * @return SUCCESS, a semantic error code, or INTERNAL_ERROR if generation fails
 */
ErrorCode semanticsRun(Node *program, SemanticError *err);
#endif // SEMANTICS_H
