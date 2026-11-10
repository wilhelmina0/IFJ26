#include "semantics.h"
#include "generator.h"
#include "symtable.h"
#include <stdlib.h>
#include <string.h>

typedef struct Scope {
  SymTable *table;
  struct Scope *parent;
} Scope;

typedef struct {
  Node *node;
  int state;
  size_t index;
  size_t low;
  bool onStack;
} FunctionInfo;

typedef struct {
  SymTable *functions;
  FunctionInfo *functionInfo;
  FunctionInfo *current;

  Scope *scope;
  Node *function;
  size_t nextId;

  FunctionInfo **componentStack;
  size_t componentCount;
  size_t nextIndex;

  SemanticError error;
} Context;

typedef struct {
  DataType type;
  bool returns;
  bool unitTail;
} Flow;

typedef struct {
  const char *name;
  DataType result;
  size_t count;
  DataType params[3];
} Builtin;

static const Builtin builtins[] = {
    {"print", TYPE_UNIT, 1, {TYPE_ANY}},
    {"readLine", TYPE_STRING, 0, {TYPE_UNKNOWN}},
    {"readInt", TYPE_INT, 0, {TYPE_UNKNOWN}},
    {"readDouble", TYPE_DOUBLE, 0, {TYPE_UNKNOWN}},
    {"toStr", TYPE_STRING, 1, {TYPE_ANY}},
    {"toInt", TYPE_INT, 1, {TYPE_DOUBLE}},
    {"strLength", TYPE_INT, 1, {TYPE_STRING}},
    {"subStr", TYPE_STRING, 3, {TYPE_STRING, TYPE_INT, TYPE_INT}},
    {"ord", TYPE_INT, 2, {TYPE_STRING, TYPE_INT}},
    {"chr", TYPE_STRING, 1, {TYPE_INT}}
};

static DataType expression(Context *ctx, Node *node,
                           bool allowCall, bool condition);
static Flow statement(Context *ctx, Node *node);
static Flow block(Context *ctx, Node *node, bool newScope);
static void check_function(Context *ctx, FunctionInfo *info);

/* Ulozi pouze prvni chybu a jeji radek. Nic nevypisuje. */
static void fail(Context *ctx, ErrorCode code, const Node *node,
                 const char *message) {
  if (ctx->error.code == SUCCESS) {
    ctx->error.code = code;
    ctx->error.line = node ? node->line : 0;
    ctx->error.message = message;
  }
}

static bool user_type(DataType type) {
  return type == TYPE_INT || type == TYPE_DOUBLE ||
         type == TYPE_STRING || type == TYPE_UNIT;
}

static bool numeric(DataType type) {
  return type == TYPE_INT || type == TYPE_DOUBLE;
}

static bool compatible(Node *value, DataType actual, DataType expected) {
  if (!value || actual == TYPE_UNKNOWN)
    return false;

  if (actual == expected || expected == TYPE_ANY)
    return true;

  if (actual == TYPE_INT && expected == TYPE_DOUBLE) {
    value->convertToDouble = true;
    return true;
  }

  return false;
}

static bool enter_scope(Context *ctx, Scope *scope, Node *node) {
  scope->table = symtable_create();

  if (!scope->table) {
    fail(ctx, INTERNAL_ERROR, node, "Nelze vytvorit tabulku symbolu.");
    return false;
  }

  scope->parent = ctx->scope;
  ctx->scope = scope;
  return true;
}

static void leave_scope(Context *ctx) {
  if (!ctx->scope)
    return;

  Scope *old = ctx->scope;
  ctx->scope = old->parent;
  symtable_destroy(old->table);
}

static bool variable(Context *ctx, const char *name, Symbol *symbol) {
  for (Scope *scope = ctx->scope; scope; scope = scope->parent) {
    if (symtable_find(scope->table, name, symbol))
      return true;
  }

  return false;
}

static void define_variable(Context *ctx, Node *node, const char *name,
                            DataType type, bool mutable) {
  if (!node || !name || !ctx->scope || ctx->nextId == 0) {
    fail(ctx, INTERNAL_ERROR, node, "Neplatna deklarace v AST.");
    return;
  }

  Symbol symbol = {
      SYMBOL_VARIABLE,
      type,
      mutable,
      ctx->nextId,
      node
  };

  ErrorCode code = symtable_insert(ctx->scope->table, name, symbol);

  if (code != SUCCESS) {
    fail(ctx, code, node, "Nelze vlozit jmeno do aktualniho rozsahu.");
  } else {
    node->symbolId = symbol.id;
    ++ctx->nextId;
  }
}

static bool term(const Node *node) {
  // TODO:
  // Rozpoznat literal nebo identifikator.
}

static DataType call(Context *ctx, Node *node) {
  // TODO:
  // Vyhledat funkci a zkontrolovat pocet a typy argumentu.
}

static DataType binary(Context *ctx, Node *node) {
  // TODO:
  // Zjistit typy operandu a overit, zda operator tyto typy podporuje.
}

static void check_function(Context *ctx, FunctionInfo *info) {
  // TODO:
  // Vytvorit rozsah pro parametry a zkontrolovat telo a navratovy typ.
}

static void register_functions(Context *ctx, Node *program) {
  // TODO:
  // Projit definice funkci a ulozit je do tabulky.
}

ErrorCode semantics_check(Node *program, SemanticError *err) {
  // TODO:
  // Inicializovat Context ctx = {0} a nastavit ctx.nextId = 1.
  // Vytvorit tabulku funkci, registrovat funkce a zkontrolovat jejich tela.
  // Uvolnit pomocne struktury, predat chybu pres err a vratit jeji kod.
}

ErrorCode semantics_run(Node *program, SemanticError *err) {
  // TODO:
  // Zavolat semantics_check.
  // Pouze pri uspechu zavolat generator.
}