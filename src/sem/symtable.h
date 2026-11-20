/**
 * @file symtable.h
 * @brief Symbol table interface
 *
 * Stores named symbols in a height-balanced binary search tree.
 * The table owns copies of names and borrows pointers to AST nodes.
 *
 * @authors
 * - Radim Winter (xwinter01)
 *
 */

#ifndef SYMTABLE_H
#define SYMTABLE_H

#include <stdbool.h>
#include <stddef.h>
#include "../ast.h"
#include "../error.h"

//typedef struct Node Node;
typedef struct SymTable SymTable;

typedef enum { SYMBOL_VARIABLE, SYMBOL_FUNCTION } SymbolKind;

/** A symbol record copied into and out of the table. */
typedef struct {
    SymbolKind kind;
    DeclType type;
    bool mutable;
    size_t id;
    Node *declaration; /**< Borrowed pointer; never freed by the table. */
} Symbol;

/**
 * @brief Creates an empty AVL symbol table.
 * @return A new table, or NULL if allocation fails.
 */
SymTable *symtableCreate(void);

/**
 * @brief Frees all entries, owned names and the table in O(n) time.
 * @param table Table to destroy; NULL has no effect. AST nodes are not freed.
 */
void symtableDestroy(SymTable *table);

/**
 * @brief Inserts a symbol and copies its name, maintaining AVL balance.
 *
 * A duplicate never overwrites the existing symbol. On any failure, the table
 * stays unchanged. Empty names are valid keys; syntax is checked by the parser.
 * Performs O(log n) key comparisons; string comparison also depends on length.
 *
 * @param table Symbol table.
 * @param name Name to copy.
 * @param value Symbol record to copy; its AST pointer remains borrowed.
 * @return SUCCESS, SEM_ERROR_REDEFINED, or INTERNAL_ERROR (including NULL input).
 */
ErrorCode symtableInsert(SymTable *table, const char *name, Symbol value);

/**
 * @brief Finds a symbol using O(log n) key comparisons and copies its record.
 *
 * Does not allocate memory. A missing name or NULL argument leaves out unchanged.
 * The output remains valid after rotations or destruction, but does not own its
 * AST node; that pointer is valid only as long as the caller keeps the AST alive.
 *
 * @param table Symbol table.
 * @param name Name to find.
 * @param out Output symbol record.
 * @return true if found, otherwise false.
 */
bool symtableFind(const SymTable *table, const char *name, Symbol *out);

#endif // SYMTABLE_H
