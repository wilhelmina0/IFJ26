/**
 * @file semantics.c
 * @brief Semantic analysis
 *
 * Checks the AST in two passes and stores semantic information
 * needed by the code generator.
 *
 * @authors
 * - Radim Winter (xwinter01)
 *
 */

#include "semantics.h"
#include "generator.h"
#include "symtable.h"
#include <stdlib.h>
#include <string.h>

// One symbol table per block, linked to its parent scope.
typedef struct Scope {
    SymTable *table;
    struct Scope *parent;
} Scope;

typedef enum { FUNCTION_NEW, FUNCTION_CHECKING, FUNCTION_CHECKED } FunctionState;

// Function analysis state, kept outside the AST.
typedef struct {
    Node *node;
    FunctionState state;
    size_t index, lowLink;
    bool onStack;
} FunctionInfo;

typedef struct {
    SymTable *functions;
    FunctionInfo *functionInfo;
    FunctionInfo *current;
    Scope *scope;
    Node *function;
    size_t nextId;
    // Stack of open call graph components (Tarjan algorithm).
    FunctionInfo **componentStack;
    size_t componentCount, nextIndex;
    SemanticError error;
} Context;

typedef struct {
    DataType type;
    bool returns;  // Whether every path through the statement returns.
    bool unitTail; // Whether all branches of a trailing if produce Unit.
} Flow;

typedef struct {
    const char *name;
    DataType result;
    size_t count;
    DataType params[3];
} Builtin;

static const Builtin builtins[] = {{"print", TYPE_UNIT, 1, {TYPE_ANY}},
                                   {"readLine", TYPE_STRING, 0, {TYPE_UNKNOWN}},
                                   {"readInt", TYPE_INT, 0, {TYPE_UNKNOWN}},
                                   {"readDouble", TYPE_DOUBLE, 0, {TYPE_UNKNOWN}},
                                   {"toStr", TYPE_STRING, 1, {TYPE_ANY}},
                                   {"toInt", TYPE_INT, 1, {TYPE_DOUBLE}},
                                   {"strLength", TYPE_INT, 1, {TYPE_STRING}},
                                   {"subStr", TYPE_STRING, 3, {TYPE_STRING, TYPE_INT, TYPE_INT}},
                                   {"ord", TYPE_INT, 2, {TYPE_STRING, TYPE_INT}},
                                   {"chr", TYPE_STRING, 1, {TYPE_INT}}};

static DataType expression(Context *ctx, Node *node, bool allowCall, bool condition);
static Flow statement(Context *ctx, Node *node);
static Flow block(Context *ctx, Node *node, bool newScope);
static void checkFunction(Context *ctx, FunctionInfo *info);

/**
 * @brief Stores the first semantic error and its source line.
 *
 * @param ctx Semantic analysis context
 * @param code Error code to store
 * @param node Source node, or NULL if the line is unknown
 * @param message Static error message; not copied or freed
 */
static void fail(Context *ctx, ErrorCode code, const Node *node, const char *message) {
    if (ctx->error.code == SUCCESS) {
        ctx->error.code = code;
        ctx->error.line = node ? node->line : 0;
        ctx->error.message = message;
    }
}

/**
 * @brief Checks whether a type is available in the basic language.
 *
 * @param type Data type to check
 * @return true for Int, Double, String or Unit, otherwise false
 */
static bool userType(DataType type) {
    return type == TYPE_INT || type == TYPE_DOUBLE || type == TYPE_STRING || type == TYPE_UNIT;
}

/**
 * @brief Checks whether a type is numeric.
 *
 * @param type Data type to check
 * @return true for Int or Double, otherwise false
 */
static bool numeric(DataType type) {
    return type == TYPE_INT || type == TYPE_DOUBLE;
}

/**
 * @brief Checks whether a value matches the expected type.
 *
 * Marks an Int to Double conversion in the AST when needed.
 *
 * @param value Expression node
 * @param actual Actual type of the expression
 * @param expected Expected data type
 * @return true if the types are compatible, otherwise false
 */
static bool compatible(Node *value, DataType actual, DataType expected) {
    if (!value || actual == TYPE_UNKNOWN) {
        return false;
    }
    if (actual == expected || expected == TYPE_ANY) {
        return true;
    }
    if (actual == TYPE_INT && expected == TYPE_DOUBLE) {
        value->convertToDouble = true;
        return true;
    }
    return false;
}

/**
 * @brief Creates a symbol table for a new scope.
 *
 * Links the new scope to its parent. The caller owns the Scope structure.
 *
 * @param ctx Semantic analysis context
 * @param scope Scope structure owned by the caller
 * @param node AST node being checked
 * @return true on success, otherwise false
 */
static bool enterScope(Context *ctx, Scope *scope, Node *node) {
    scope->table = symtableCreate();
    if (!scope->table) {
        fail(ctx, INTERNAL_ERROR, node, "Cannot create symbol table.");
        return false;
    }
    scope->parent = ctx->scope;
    ctx->scope = scope;
    return true;
}

/**
 * @brief Destroys the current symbol table and restores its parent scope.
 *
 * Does not free the Scope structure or AST nodes.
 *
 * @param ctx Semantic analysis context
 */
static void leaveScope(Context *ctx) {
    if (!ctx->scope) {
        return;
    }
    Scope *old = ctx->scope;
    ctx->scope = old->parent;
    symtableDestroy(old->table);
}

/**
 * @brief Looks up the nearest visible variable.
 *
 * Searches the current scope first, then continues through parent scopes.
 *
 * @param ctx Semantic analysis context
 * @param name Symbol name
 * @param symbol Output symbol record
 * @return true if the variable was found, otherwise false
 */
static bool variable(Context *ctx, const char *name, Symbol *symbol) {
    for (Scope *scope = ctx->scope; scope; scope = scope->parent) {
        if (symtableFind(scope->table, name, symbol)) {
            return true;
        }
    }
    return false;
}

/**
 * @brief Inserts a variable into the current scope.
 *
 * Stores its type, mutability and identifier, and writes the identifier to the AST.
 * The symbol table rejects duplicate names in the same scope.
 *
 * @param ctx Semantic analysis context
 * @param node AST node being checked
 * @param name Symbol name
 * @param type Data type to check
 * @param mutable Whether the variable can be reassigned
 */
static void defineVariable(Context *ctx, Node *node, const char *name, DataType type,
                           bool mutable) {
    if (!node || !name || !ctx->scope || ctx->nextId == 0) {
        fail(ctx, INTERNAL_ERROR, node, "Invalid declaration in AST.");
        return;
    }
    Symbol symbol = {SYMBOL_VARIABLE, type, mutable, ctx->nextId, node};
    ErrorCode code = symtableInsert(ctx->scope->table, name, symbol);
    if (code != SUCCESS) {
        fail(ctx, code, node, "Cannot insert name into current scope.");
    } else {
        node->symbolId = symbol.id;
        ++ctx->nextId;
    }
}

/**
 * @brief Checks whether a node is a valid argument in the basic language.
 *
 * Accepts identifiers, literals and signed numeric literals.
 *
 * @param node AST node being checked
 * @return true for a valid term, otherwise false
 */
static bool term(const Node *node) {
    if (!node) {
        return false;
    }
    if (node->type == AST_IDENTIFIER || node->type == AST_INT || node->type == AST_DOUBLE ||
        node->type == AST_STRING || node->type == AST_UNIT) {
        return true;
    }
    return node->type == AST_UNARY && node->childCount == 1 && node->children &&
           node->children[0] &&
           (node->data.unary.op == TOKEN_MINUS || node->data.unary.op == TOKEN_PLUS) &&
           (node->children[0]->type == AST_INT || node->children[0]->type == AST_DOUBLE);
}

/**
 * @brief Checks a function call and its arguments.
 *
 * Resolves the function and checks the argument count and types.
 * Analyzes the called function when its return type is not known yet.
 *
 * @param ctx Semantic analysis context
 * @param node AST node being checked
 * @return return type of the function, or TYPE_UNKNOWN on failure
 */
static DataType call(Context *ctx, Node *node) {
    const char *name = node->data.call.id;
    if (!name) {
        fail(ctx, INTERNAL_ERROR, node, "Function call has no name.");
        return TYPE_UNKNOWN;
    }
    Symbol symbol;
    if (variable(ctx, name, &symbol)) {
        fail(ctx, SEM_ERROR_UNDEFINED, node, "Local name does not refer to a callable function.");
        return TYPE_UNKNOWN;
    }
    Node *function = NULL;
    FunctionInfo *info = NULL;
    const Builtin *builtin = NULL;
    if (symtableFind(ctx->functions, name, &symbol)) {
        node->symbolId = symbol.id;
        function = symbol.declaration;
        info = &ctx->functionInfo[symbol.id - 1];
    } else {
        for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); ++i) {
            if (strcmp(name, builtins[i].name) == 0) {
                builtin = &builtins[i];
                break;
            }
        }
    }
    if (!function && !builtin) {
        fail(ctx, SEM_ERROR_UNDEFINED, node, "Undefined function.");
        return TYPE_UNKNOWN;
    }
    size_t count = function ? function->childCount - 1 : builtin->count;
    if (node->childCount != count) {
        fail(ctx, SEM_ERROR_INVALID, node, "Invalid number of arguments.");
        return TYPE_UNKNOWN;
    }
    for (size_t i = 0; i < count; ++i) {
        Node *arg = node->children[i];
        if (!term(arg)) {
            fail(ctx, SYN_ERROR, arg, "Arguments must be terms without the FUNEXP extension.");
            return TYPE_UNKNOWN;
        }
        DataType actual = expression(ctx, arg, false, false);
        DataType expected =
            function ? function->children[i]->data.parameter.declaredType : builtin->params[i];
        if (!compatible(arg, actual, expected)) {
            fail(ctx, SEM_ERROR_INVALID, arg, "Invalid argument type.");
        }
    }
    if (ctx->error.code != SUCCESS) {
        return TYPE_UNKNOWN;
    }
    if (!function) {
        return builtin->result;
    }
    if (info->state == FUNCTION_CHECKING) {
        if (function->data.function.returnType == TYPE_UNKNOWN) {
            fail(ctx, SEM_ERROR_NOMATCH, function, "Recursion requires an explicit return type.");
        }
        if (info->index < ctx->current->lowLink) {
            ctx->current->lowLink = info->index;
        }
        return function->data.function.returnType;
    }
    bool fresh = info->state == FUNCTION_NEW;
    checkFunction(ctx, info);
    if (info->onStack) {
        size_t lowLink = fresh ? info->lowLink : info->index;
        if (lowLink < ctx->current->lowLink) {
            ctx->current->lowLink = lowLink;
        }
    }
    return function->valueType;
}

/**
 * @brief Checks operand types of a binary expression.
 *
 * Marks numeric conversions when needed. Actual values are evaluated at runtime.
 *
 * @param ctx Semantic analysis context
 * @param node AST node being checked
 * @return type of the result, or TYPE_UNKNOWN on failure
 */
static DataType binary(Context *ctx, Node *node) {
    Node *left = node->data.binary.left;
    Node *right = node->data.binary.right;
    DataType leftType = expression(ctx, left, false, false);
    if (ctx->error.code != SUCCESS) {
        return TYPE_UNKNOWN;
    }
    DataType rightType = expression(ctx, right, false, false);
    if (ctx->error.code != SUCCESS) {
        return TYPE_UNKNOWN;
    }
    TokenType op = node->data.binary.op;
    bool numbers = numeric(leftType) && numeric(rightType);
    bool strings = leftType == TYPE_STRING && rightType == TYPE_STRING;
    if (numbers && leftType != rightType) {
        if (leftType == TYPE_INT) {
            left->convertToDouble = true;
        } else {
            right->convertToDouble = true;
        }
    }
    switch (op) {
        case TOKEN_PLUS:
            if (strings) {
                return TYPE_STRING;
            }
            /* fall through */
        case TOKEN_MINUS:
        case TOKEN_MUL:
        case TOKEN_DIV:
            if (numbers) {
                return (leftType == TYPE_DOUBLE || rightType == TYPE_DOUBLE) ? TYPE_DOUBLE
                                                                             : TYPE_INT;
            }
            break;
        case TOKEN_LT:
        case TOKEN_LTE:
        case TOKEN_GT:
        case TOKEN_GTE:
            if (numbers || strings) {
                return TYPE_BOOL;
            }
            break;
        case TOKEN_EQ:
        case TOKEN_NEQ:
            if (numbers || leftType == rightType) {
                return TYPE_BOOL;
            }
            break;
        default:
            fail(ctx, INTERNAL_ERROR, node, "Unknown operator in AST.");
            return TYPE_UNKNOWN;
    }
    fail(ctx, SEM_ERROR_NOMATCH, node, "Incompatible operand types.");
    return TYPE_UNKNOWN;
}

/**
 * @brief Checks an expression and stores its type in the AST.
 *
 * Resolves variable identifiers and checks whether calls are allowed here.
 *
 * @param ctx Semantic analysis context
 * @param node AST node being checked
 * @param allowCall Whether a function call is allowed here
 * @param condition Whether the expression is used as a condition
 * @return type of the expression; ctx->error records any semantic error
 */
static DataType expression(Context *ctx, Node *node, bool allowCall, bool condition) {
    if (ctx->error.code != SUCCESS) {
        return TYPE_UNKNOWN;
    }
    if (!node) {
        fail(ctx, INTERNAL_ERROR, node, "Missing expression in AST.");
        return TYPE_UNKNOWN;
    }
    node->convertToDouble = false;
    node->symbolId = 0;
    DataType type = TYPE_UNKNOWN;
    Symbol symbol;
    switch (node->type) {
        case AST_INT:
            type = TYPE_INT;
            break;
        case AST_DOUBLE:
            type = TYPE_DOUBLE;
            break;
        case AST_STRING:
            type = TYPE_STRING;
            break;
        case AST_UNIT:
            type = TYPE_UNIT;
            break;
        case AST_IDENTIFIER:
            if (!node->data.identifier.id) {
                fail(ctx, INTERNAL_ERROR, node, "Identifier has no name.");
            } else if (!variable(ctx, node->data.identifier.id, &symbol)) {
                fail(ctx, SEM_ERROR_UNDEFINED, node, "Undefined variable.");
            } else {
                type = symbol.type;
                node->symbolId = symbol.id;
            }
            break;
        case AST_CALL:
            if (!allowCall) {
                fail(ctx, SYN_ERROR, node,
                     "Calls inside expressions require the FUNEXP extension.");
            } else {
                type = call(ctx, node);
            }
            break;
        case AST_BINARY:
            type = binary(ctx, node);
            break;
        case AST_UNARY:
            if (node->childCount != 1) {
                fail(ctx, INTERNAL_ERROR, node, "Invalid unary expression in AST.");
                break;
            }
            type = expression(ctx, node->children[0], false, false);
            if ((node->data.unary.op != TOKEN_MINUS && node->data.unary.op != TOKEN_PLUS) ||
                !numeric(type)) {
                fail(ctx, SEM_ERROR_NOMATCH, node, "Unary operator requires a numeric operand.");
            }
            break;
        default:
            fail(ctx, SYN_ERROR, node, "Expected an expression.");
            break;
    }
    if (type == TYPE_BOOL && !condition) {
        fail(ctx, SEM_ERROR_NOMATCH, node, "Boolean values are only allowed in conditions.");
    }
    node->valueType = type;
    return type;
}

/**
 * @brief Checks that an if or while condition has type Boolean.
 *
 * @param ctx Semantic analysis context
 * @param node AST node being checked
 */
static void condition(Context *ctx, Node *node) {
    if (expression(ctx, node, false, true) != TYPE_BOOL) {
        fail(ctx, SEM_ERROR_NOMATCH, node, "Condition must have type Boolean.");
    }
}

/**
 * @brief Checks a return statement against the declared return type.
 *
 * Also checks return statements in unreachable code.
 *
 * @param ctx Semantic analysis context
 * @param node AST node being checked
 * @return type and control flow information for the statement
 */
static Flow returnStatement(Context *ctx, Node *node) {
    Flow flow = {TYPE_UNIT, true, true};
    if (node->childCount > 1) {
        fail(ctx, INTERNAL_ERROR, node, "Invalid return statement in AST.");
        return flow;
    }
    DataType expected = ctx->function->data.function.returnType;
    if (expected == TYPE_UNKNOWN) {
        fail(ctx, SEM_ERROR_NOMATCH, node, "Return requires an explicit function return type.");
        return flow;
    }
    if (!node->childCount) {
        if (expected != TYPE_UNIT) {
            fail(ctx, SEM_ERROR_NOMATCH, node, "Return without a value requires Unit.");
        }
    } else {
        Node *value = node->children[0];
        DataType actual = expression(ctx, value, true, false);
        if (expected != TYPE_UNIT && !compatible(value, actual, expected)) {
            fail(ctx, SEM_ERROR_NOMATCH, node, "Incompatible return type.");
        }
    }
    return flow;
}

/**
 * @brief Checks one statement and updates its AST node.
 *
 * Checks a variable initializer before inserting the declaration into its scope.
 *
 * @param ctx Semantic analysis context
 * @param node AST node being checked
 * @return type and control flow information for the statement
 */
static Flow statement(Context *ctx, Node *node) {
    Flow flow = {TYPE_UNIT, false, true};
    if (ctx->error.code != SUCCESS) {
        return flow;
    }
    if (!node) {
        fail(ctx, INTERNAL_ERROR, node, "Missing statement.");
        return flow;
    }
    node->symbolId = 0;
    node->convertToDouble = false;
    Symbol symbol;
    switch (node->type) {
        case AST_DECL: {
            const char *name = node->data.declaration.name;
            if (!name) {
                fail(ctx, INTERNAL_ERROR, node, "Declaration has no name.");
                break;
            }
            if (symtableFind(ctx->scope->table, name, &symbol)) {
                fail(ctx, SEM_ERROR_REDEFINED, node, "Duplicate definition in the same block.");
                break;
            }
            DataType expected = node->data.declaration.declaredType;
            if (expected != TYPE_UNKNOWN && !userType(expected)) {
                fail(ctx, SEM_ERROR_UNDEFINED, node, "Invalid declared type.");
                break;
            }
            Node *value = node->data.declaration.value;
            DataType actual = expression(ctx, value, true, false);
            if (expected == TYPE_UNKNOWN) {
                expected = actual;
            }
            if (!compatible(value, actual, expected)) {
                fail(ctx, SEM_ERROR_NOMATCH, node, "Incompatible initializer type.");
            }
            if (ctx->error.code == SUCCESS) {
                defineVariable(ctx, node, name, expected, node->data.declaration.mutable);
            }
            // For a declaration, valueType is the variable type, not the statement result.
            node->valueType = expected;
            return flow;
        }
        case AST_ASSIGN: {
            if (!node->data.assignment.name) {
                fail(ctx, INTERNAL_ERROR, node, "Assignment has no target name.");
                break;
            }
            if (!variable(ctx, node->data.assignment.name, &symbol)) {
                fail(ctx, SEM_ERROR_UNDEFINED, node, "Assignment to an undefined variable.");
                break;
            }
            if (!symbol.mutable) {
                fail(ctx, SEM_ERROR_VAL, node, "Cannot assign to a val or parameter.");
                break;
            }
            Node *value = node->data.assignment.value;
            DataType actual = expression(ctx, value, true, false);
            if (!compatible(value, actual, symbol.type)) {
                fail(ctx, SEM_ERROR_NOMATCH, node, "Incompatible assignment type.");
            }
            node->symbolId = symbol.id;
            break;
        }
        case AST_RETURN:
            flow = returnStatement(ctx, node);
            break;
        case AST_IF: {
            if (node->childCount != 3) {
                fail(ctx, SYN_ERROR, node,
                     "An if statement requires both branches in the basic language.");
                break;
            }
            condition(ctx, node->children[0]);
            if (ctx->error.code != SUCCESS) {
                break;
            }
            Flow yes = block(ctx, node->children[1], true);
            Flow no = block(ctx, node->children[2], true);
            flow.returns = yes.returns && no.returns;
            flow.unitTail =
                yes.unitTail && no.unitTail && yes.type == TYPE_UNIT && no.type == TYPE_UNIT;
            break;
        }
        case AST_WHILE:
            if (node->childCount != 2) {
                fail(ctx, INTERNAL_ERROR, node, "Invalid while statement in AST.");
                break;
            }
            condition(ctx, node->children[0]);
            if (ctx->error.code != SUCCESS) {
                break;
            }
            (void)block(ctx, node->children[1], true);
            // A loop may execute zero times even if its body returns.
            break;
        case AST_BLOCK:
            fail(ctx, SYN_ERROR, node,
                 "Standalone nested blocks are not supported in basic IFJ26.");
            break;
        default:
            flow.type = expression(ctx, node, true, false);
            flow.unitTail = flow.type == TYPE_UNIT;
            return flow;
    }
    node->valueType = flow.type;
    return flow;
}

/**
 * @brief Checks statements in a block in source order.
 *
 * Checks unreachable statements as well. The syntactically last statement
 * determines the block result used when checking the function return type.
 *
 * @param ctx Semantic analysis context
 * @param node AST node being checked
 * @param newScope Whether to create a separate scope for this block
 * @return type and control flow information for the block
 */
static Flow block(Context *ctx, Node *node, bool newScope) {
    Flow flow = {TYPE_UNIT, false, true};
    if (ctx->error.code != SUCCESS) {
        return flow;
    }
    if (!node || node->type != AST_BLOCK || !node->childCount) {
        fail(ctx, SYN_ERROR, node, "Expected a nonempty block.");
        return flow;
    }
    Scope scope;
    if (newScope && !enterScope(ctx, &scope, node)) {
        return flow;
    }
    for (size_t i = 0; i < node->childCount && ctx->error.code == SUCCESS; ++i) {
        flow = statement(ctx, node->children[i]);
    }
    node->valueType = flow.type;
    if (newScope) {
        leaveScope(ctx);
    }
    return flow;
}

/**
 * @brief Checks the final result and inferred return type of a function.
 *
 * @param ctx Semantic analysis context
 * @param function Function being checked
 * @param body Function body
 * @param result Result of checking the body
 */
static void checkFunctionResult(Context *ctx, Node *function, Node *body, Flow result) {
    DataType expected = function->data.function.returnType;
    Node *last = body;
    if (body->type == AST_BLOCK && body->childCount > 0) {
        last = body->children[body->childCount - 1];
    }
    if (ctx->error.code == SUCCESS && !result.returns) {
        if (last->type == AST_IF && expected != TYPE_UNIT &&
            !(expected == TYPE_UNKNOWN && result.unitTail)) {
            fail(ctx, SEM_ERROR_NOMATCH, last, "Invalid trailing if statement in function body.");
        } else if (expected == TYPE_UNKNOWN) {
            expected = result.type;
        } else if (expected != TYPE_UNIT && !compatible(last, result.type, expected)) {
            fail(ctx, SEM_ERROR_NOMATCH, last, "Incompatible function body result.");
        }
    }
    function->valueType = expected;
    if (ctx->error.code == SUCCESS && function->data.function.main && expected != TYPE_UNIT) {
        fail(ctx, SEM_ERROR_NOMATCH, function, "Entry function must return Unit.");
    }
}

/**
 * @brief Finishes a call graph component and checks recursive return types.
 *
 * A cycle can include functions visited through different DFS branches.
 * The component stack keeps these functions together until their root is done.
 *
 * @param ctx Semantic analysis context
 * @param info Function whose analysis has just finished
 */
static void checkRecursion(Context *ctx, FunctionInfo *info) {
    if (info->lowLink != info->index) {
        return;
    }
    size_t end = ctx->componentCount;
    FunctionInfo *item;
    do {
        item = ctx->componentStack[--ctx->componentCount];
        item->onStack = false;
    } while (item != info);
    if (end - ctx->componentCount > 1) {
        for (size_t i = ctx->componentCount; i < end; ++i) {
            item = ctx->componentStack[i];
            if (item->node->data.function.returnType == TYPE_UNKNOWN) {
                fail(ctx, SEM_ERROR_NOMATCH, item->node,
                     "Recursion requires an explicit return type.");
            }
        }
    }
}

/**
 * @brief Checks a function body once and determines its return type.
 *
 * Creates a separate scope for parameters and local variables.
 * Tracks call graph components to check recursive functions.
 *
 * @param ctx Semantic analysis context
 * @param info Analysis state of the function
 */
static void checkFunction(Context *ctx, FunctionInfo *info) {
    Node *function = info->node;
    if (ctx->error.code != SUCCESS || info->state == FUNCTION_CHECKED) {
        return;
    }
    info->state = FUNCTION_CHECKING;
    info->index = ctx->nextIndex++;
    info->lowLink = info->index;
    info->onStack = true;
    ctx->componentStack[ctx->componentCount++] = info;
    Scope *savedScope = ctx->scope;
    Node *savedFunction = ctx->function;
    FunctionInfo *savedInfo = ctx->current;
    ctx->scope = NULL;
    ctx->function = function;
    ctx->current = info;
    Scope locals;
    if (enterScope(ctx, &locals, function)) {
        for (size_t i = 0; i + 1 < function->childCount && ctx->error.code == SUCCESS; ++i) {
            Node *param = function->children[i];
            param->valueType = param->data.parameter.declaredType;
            defineVariable(ctx, param, param->data.parameter.name, param->valueType, false);
        }
        Node *body = function->children[function->childCount - 1];
        Flow result;
        if (body->type == AST_BLOCK) {
            result = block(ctx, body, false);
        } else {
            DataType type = expression(ctx, body, true, false);
            result = (Flow){type, false, type == TYPE_UNIT};
        }
        checkFunctionResult(ctx, function, body, result);
        leaveScope(ctx);
    }
    ctx->scope = savedScope;
    ctx->function = savedFunction;
    ctx->current = savedInfo;
    info->state = FUNCTION_CHECKED;
    checkRecursion(ctx, info);
}

/**
 * @brief Registers function signatures during the first pass.
 *
 * Checks signature types, duplicate definitions and the entry function.
 * Registering all functions first allows calls before their definitions.
 *
 * @param ctx Semantic analysis context
 * @param program Root AST_PROGRAM node
 */
static void firstPass(Context *ctx, Node *program) {
    size_t mainCount = 0;
    for (size_t i = 0; i < program->childCount && ctx->error.code == SUCCESS; ++i) {
        Node *fn = program->children[i];
        if (!fn || fn->type != AST_FUNCTION || !fn->data.function.id || !fn->childCount) {
            fail(ctx, INTERNAL_ERROR, fn, "Invalid function in AST.");
            break;
        }
        ctx->functionInfo[i].node = fn;
        fn->valueType = TYPE_UNKNOWN;
        fn->symbolId = i + 1;
        DataType type = fn->data.function.returnType;
        if (type != TYPE_UNKNOWN && !userType(type)) {
            fail(ctx, SEM_ERROR_UNDEFINED, fn, "Invalid return type.");
            break;
        }
        for (size_t j = 0; j + 1 < fn->childCount; ++j) {
            Node *param = fn->children[j];
            if (!param || param->type != AST_PARAM || !param->data.parameter.name) {
                fail(ctx, INTERNAL_ERROR, fn, "Invalid parameter in AST.");
                break;
            }
            if (!userType(param->data.parameter.declaredType)) {
                fail(ctx, SEM_ERROR_UNDEFINED, param, "Invalid parameter type.");
            }
        }
        if (ctx->error.code != SUCCESS) {
            break;
        }
        Symbol symbol = {SYMBOL_FUNCTION, type, false, i + 1, fn};
        ErrorCode code = symtableInsert(ctx->functions, fn->data.function.id, symbol);
        if (code != SUCCESS) {
            fail(ctx, code, fn, "Duplicate function or symbol table error.");
        }
        if (fn->data.function.main) {
            ++mainCount;
            if (mainCount > 1) {
                fail(ctx, SEM_ERROR_REDEFINED, fn, "Multiple entry functions.");
            } else if (fn->childCount != 1) {
                fail(ctx, SEM_ERROR_INVALID, fn, "Entry function must not have parameters.");
            } else if (type != TYPE_UNKNOWN && type != TYPE_UNIT) {
                fail(ctx, SEM_ERROR_NOMATCH, fn, "Entry function must return Unit.");
            }
        }
    }
    if (!mainCount) {
        fail(ctx, SEM_ERROR_UNDEFINED, program, "Missing entry function.");
    }
}

/**
 * @brief Checks function bodies during the second pass.
 *
 * Checks local declarations, expressions and return types. Function bodies
 * may be visited in dependency order when inferring return types.
 *
 * @param ctx Semantic analysis context
 * @param program Root AST_PROGRAM node
 */
static void secondPass(Context *ctx, Node *program) {
    for (size_t i = 0; i < program->childCount && ctx->error.code == SUCCESS; ++i) {
        checkFunction(ctx, &ctx->functionInfo[i]);
    }
}

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
ErrorCode semanticsCheck(Node *program, SemanticError *err) {
    Context ctx = {0};
    ctx.nextId = 1;
    ctx.error.message = "OK";

    if (!program || program->type != AST_PROGRAM) {
        fail(&ctx, INTERNAL_ERROR, program, "Expected an AST_PROGRAM root.");
    }

    if (ctx.error.code == SUCCESS) {
        ctx.functions = symtableCreate();
        if (!ctx.functions) {
            fail(&ctx, INTERNAL_ERROR, program, "Cannot create function table.");
        }
    }

    if (ctx.error.code == SUCCESS) {
        size_t count = program->childCount;
        if (count == 0) {
            count = 1;
        }
        ctx.componentStack = calloc(count, sizeof(*ctx.componentStack));
        ctx.functionInfo = calloc(count, sizeof(*ctx.functionInfo));
        if (!ctx.componentStack || !ctx.functionInfo) {
            fail(&ctx, INTERNAL_ERROR, program, "Cannot allocate function analysis state.");
        }
    }

    if (ctx.error.code == SUCCESS) {
        firstPass(&ctx, program);
    }
    if (ctx.error.code == SUCCESS) {
        secondPass(&ctx, program);
    }

    free(ctx.componentStack);
    free(ctx.functionInfo);
    symtableDestroy(ctx.functions);

    if (err) {
        *err = ctx.error;
    }
    return ctx.error.code;
}

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
ErrorCode semanticsRun(Node *program, SemanticError *err) {
    ErrorCode code = semanticsCheck(program, err);
    if (code != SUCCESS) {
        return code;
    }

    code = generateCode(program);
    if (code != SUCCESS) {
        if (err) {
            err->code = INTERNAL_ERROR;
            err->line = 0;
            err->message = "Code generation failed.";
        }
        return INTERNAL_ERROR;
    }
    return SUCCESS;
}
