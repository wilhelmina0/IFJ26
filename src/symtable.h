/**
 * @file symtable.h
 * @brief symbol table interface
 *
 * @authors
 * - Radim Winter
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include "ast.h"
#include "error.h"
#include <stdbool.h>
#include <stddef.h>
#include "ast.h"

typedef struct SymTable SymTable;

typedef enum { SYMBOL_VARIABLE, SYMBOL_FUNCTION } SymbolKind;

// variable or function data stored in the table
typedef struct Symbol {
    SymbolKind kind;
    DeclType type;
    bool mutable;
    size_t id;
    Node *declaration; // declaration node owned by the tree of the program
} Symbol;

SymTable *symtableCreate(void);

void symtableDestroy(SymTable *table);

ErrorCode symtableInsert(SymTable *table, const char *name, Symbol value);

bool symtableFind(const SymTable *table, const char *name, Symbol *out);

#endif // SYMTABLE_H
