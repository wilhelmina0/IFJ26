#include "symtable.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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
  size_t hash = 5381;
  while (*name) {
    hash = hash * 33 + (unsigned char)*name;
    ++name;
  }
  return hash;
}

static size_t find_slot(const Entry *entries, size_t capacity,
                        const char *name) {
  size_t index = hash_name(name) % capacity;
  for (size_t tried = 0; tried < capacity; ++tried) {
    if (!entries[index].name || strcmp(entries[index].name, name) == 0)
      return index;
    ++index;
    if (index == capacity)
      index = 0;
  }
  return SIZE_MAX;
}

static bool grow(SymTable *table) {
  if (table->capacity > SIZE_MAX / 2 / sizeof(Entry))
    return false;
  size_t capacity = table->capacity * 2;
  Entry *entries = calloc(capacity, sizeof(*entries));
  if (!entries)
    return false;
  for (size_t i = 0; i < table->capacity; ++i) {
    if (table->entries[i].name) {
      size_t index = find_slot(entries, capacity, table->entries[i].name);
      entries[index] = table->entries[i];
    }
  }
  free(table->entries);
  table->entries = entries;
  table->capacity = capacity;
  return true;
}

SymTable *symtable_create(void) {
  SymTable *table = malloc(sizeof(*table));
  if (!table)
    return NULL;
  table->capacity = 16;
  table->count = 0;
  table->entries = calloc(table->capacity, sizeof(*table->entries));
  if (!table->entries) {
    free(table);
    return NULL;
  }
  return table;
}

void symtable_destroy(SymTable *table) {
  if (!table)
    return;
  for (size_t i = 0; i < table->capacity; ++i)
    free(table->entries[i].name);
  free(table->entries);
  free(table);
}

bool symtable_find(const SymTable *table, const char *name, Symbol *out) {
  if (!table || !name || !out)
    return false;
  size_t index = find_slot(table->entries, table->capacity, name);
  if (index == SIZE_MAX || !table->entries[index].name)
    return false;
  *out = table->entries[index].value;
  return true;
}

ErrorCode symtable_insert(SymTable *table, const char *name, Symbol value) {
  if (!table || !name)
    return INTERNAL_ERROR;
  size_t index = find_slot(table->entries, table->capacity, name);
  if (index != SIZE_MAX && table->entries[index].name)
    return SEM_ERROR_REDEFINED;

  size_t length = strlen(name);
  if (length == SIZE_MAX)
    return INTERNAL_ERROR;
  char *copy = malloc(length + 1);
  if (!copy)
    return INTERNAL_ERROR;
  memcpy(copy, name, length + 1);

  if (table->count >= table->capacity - table->capacity / 4) {
    if (!grow(table)) {
      free(copy);
      return INTERNAL_ERROR;
    }
    index = find_slot(table->entries, table->capacity, name);
  }
  if (index == SIZE_MAX) {
    free(copy);
    return INTERNAL_ERROR;
  }
  table->entries[index].name = copy;
  table->entries[index].value = value;
  ++table->count;
  return SUCCESS;
}