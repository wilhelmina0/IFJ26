#include "symtable.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* Jedna polozka tabulky. */
typedef struct {
  char *name;
  Symbol value;
} Entry;

struct SymTable {
  Entry *entries;
  size_t capacity;
  size_t count;
};

static size_t hash_name(const char *name) {
  // TODO:
  // projit znaky, ulozit jako unsigned char 
}

,
static size_t find_slot(const Entry *entries, size_t capacity,
                        const char *name) {
  // TODO:
  //vypocet pocatecniho indexu
}

static bool grow(SymTable *table) {
  // TODO:
  // alokace zvetseneho pole
}

SymTable *symtable_create(void) {
  // TODO:
  //alokace tabulky atd
}

void symtable_destroy(SymTable *table) {
  // TODO:
  // free atd
}

bool symtable_find(const SymTable *table, const char *name, Symbol *out) {
  // TODO:
  //hledani symbolu
}

ErrorCode symtable_insert(SymTable *table, const char *name, Symbol value) {
  // TODO:
}