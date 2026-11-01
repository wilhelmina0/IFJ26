#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "error.h"
#include <stdbool.h>
#include <stddef.h>

typedef enum {
  TYPE_UNKNOWN,
  TYPE_INT,
  TYPE_DOUBLE,
  TYPE_STRING,
  TYPE_UNIT,
  TYPE_BOOL,
  TYPE_ANY
} DataType;

typedef struct Node Node;
typedef struct SymTable SymTable;

typedef enum { SYMBOL_VARIABLE, SYMBOL_FUNCTION } SymbolKind;
typedef struct {
  SymbolKind kind;
  DataType type;
  bool mutable;
  size_t id;
  Node *declaration;
} Symbol;

SymTable *symtable_create(void);

void symtable_destroy(SymTable *table);

ErrorCode symtable_insert(SymTable *table, const char *name, Symbol value);

bool symtable_find(const SymTable *table, const char *name, Symbol *out);

#endif
