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

static void fail(Context *ctx, ErrorCode code, const Node *node,
                 const char *message) {
  // TODO:
  // vypsani chyby a radku
}

static bool user_type(DataType type) {
  // TODO:
  //rozpoznat typ
}

static bool numeric(DataType type) {
  // TODO:
  // rozpozna int a double jinak char
}

static bool compatible(Node *value, DataType actual, DataType expected) {
  // TODO:
  //overeni shody typu pripadne convert int to double
}

static bool enter_scope(Context *ctx, Scope *scope, Node *node) {
  // TODO:
  //vytvoreni tabulky pro novy rosah
}

static void leave_scope(Context *ctx) {
  // TODO:
  //free tabulky
}

static bool variable(Context *ctx, const char *name, Symbol *symbol) {
  // TODO:
  //ukladani symbolu
}

static void define_variable(Context *ctx, Node *node, const char *name,
                            DataType type, bool mutable) {
  // TODO:
  //vytvori k symbolu jeho typ a id
}

static bool term(const Node *node) {
  // TODO:
  // Rozpoznat literal nebo identifikator
}

static DataType call(Context *ctx, Node *node) {
  // TODO:
  //funkce
}

static DataType binary(Context *ctx, Node *node) {
  // TODO:
  //zjistit typ operandu
}


static void check_function(Context *ctx, FunctionInfo *info) {
  // TODO:
  //ulozeni funkce
}

static void register_functions(Context *ctx, Node *program) {
  // TODO:
  //projit definice funkci a ulozit je do tabulky.
}

ErrorCode semantics_check(Node *program, SemanticError *err) {
  // TODO:
  //inicializace, vytvoreni, kontrola
}

ErrorCode semantics_run(Node *program, SemanticError *err) {
  // TODO:
  //zavolat check a pak generator pokud vse ok
}