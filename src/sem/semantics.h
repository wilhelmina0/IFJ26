#ifndef SEMANTICS_H
#define SEMANTICS_H
#include "ast.h"
#include "error.h"
#include "symtable.h"

typedef struct {
  ErrorCode code;
  int line;
  const char *message; /* Staticky text, neuvolnovat. */
} SemanticError;

ErrorCode semantics_check(Node *program, SemanticError *err);

ErrorCode semantics_run(Node *program, SemanticError *err);
#endif
