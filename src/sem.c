/**
 * @file sem.c
 * @brief semantic analysis
 *
 * @authors
 * - Radim Winter
 */

#include "sem.h"
#include "ast.h"
#include "symtable.h"
#include <stdlib.h>
#include <string.h>

// local symbol tables linked to outer scopes
typedef struct Scope {
    SymTable *table;
    struct Scope *parent;
} Scope;

typedef enum { FUNCTION_NEW, FUNCTION_CHECKING, FUNCTION_CHECKED } FunctionState;

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

    // function stack for detecting recursive groups
    FunctionInfo **componentStack;
    size_t componentCount, nextIndex;
} AnalysisState;

typedef struct {
    DeclType type;
    // explicit return on every branch
    bool returns;
    // unit result at the end of every branch
    bool unitTail;
} Flow;

typedef struct {
    const char *name;
    DeclType result;
    size_t count;
    DeclType params[3];
} Builtin;

// names, return types and parameters of built-ins
static const Builtin builtins[] = {
    {"print", TYPE_UNIT, 1, {TYPE_ANY}},
    {"readLine", TYPE_STRING, 0, {TYPE_UNSPECIFIED}},
    {"readInt", TYPE_INT, 0, {TYPE_UNSPECIFIED}},
    {"readDouble", TYPE_DOUBLE, 0, {TYPE_UNSPECIFIED}},
    {"toStr", TYPE_STRING, 1, {TYPE_ANY}},
    {"toInt", TYPE_INT, 1, {TYPE_DOUBLE}},
    {"strLength", TYPE_INT, 1, {TYPE_STRING}},
    {"substr", TYPE_STRING, 3, {TYPE_STRING, TYPE_INT, TYPE_INT}},
    {"ord", TYPE_INT, 2, {TYPE_STRING, TYPE_INT}},
    {"chr", TYPE_STRING, 1, {TYPE_INT}},
};

static DeclType checkExpression(AnalysisState *state, Error *error, Node *node, bool allowCall, bool inCondition);
static Flow checkStatement(AnalysisState *state, Error *error, Node *node);
static Flow checkBlock(AnalysisState *state, Error *error, Node *node, bool newScope);
static void checkFunction(AnalysisState *state, Error *error, FunctionInfo *info);

static bool isUserType(DeclType type) {
    return type == TYPE_INT || type == TYPE_DOUBLE || type == TYPE_STRING || type == TYPE_UNIT;
}

static bool isNumeric(DeclType type) { return type == TYPE_INT || type == TYPE_DOUBLE; }

static bool compatible(Node *value, DeclType actual, DeclType expected) {
    if (!value || actual == TYPE_UNSPECIFIED) {
        return false;
    }
    if (actual == expected || expected == TYPE_ANY) {
        return true;
    }
    if (actual == TYPE_INT && expected == TYPE_DOUBLE) {
        // mark int to double conversion for the generator
        value->convertToDouble = true;
        return true;
    }
    return false;
}

static bool initScope(AnalysisState *state, Error *error, Scope *scope, Node *node) {
    scope->table = symtableCreate();
    if (!scope->table) {
        handleError(error, INTERNAL_ERROR, "Cannot create symbol table.", node ? node->line : 0);
        return false;
    }
    scope->parent = state->scope;
    return true;
}

static bool findVariable(AnalysisState *state, const char *name, Symbol *symbol) {
    // search from the current scope towards outer scopes
    for (Scope *scope = state->scope; scope; scope = scope->parent) {
        if (symtableFind(scope->table, name, symbol)) {
            return true;
        }
    }
    return false;
}

static void defineVariable(AnalysisState *state, Error *error, Node *node, const char *name, DeclType type,
                           bool mutable) {
    if (!node || !name || !state->scope || state->nextId == 0) {
        handleError(error, INTERNAL_ERROR, "Invalid declaration in AST.", node ? node->line : 0);
        return;
    }
    Symbol symbol = {SYMBOL_VARIABLE, type, mutable, state->nextId, node};
    Symbol *owned = malloc(sizeof(*owned));
    if (!owned) {
        handleError(error, INTERNAL_ERROR, "Cannot allocate symbol record.", node ? node->line : 0);
        return;
    }
    *owned = symbol;
    ErrorCode code = symtableInsert(state->scope->table, name, symbol);
    if (code != SUCCESS) {
        free(owned);
        handleError(error, code, "Cannot insert name into current scope.", node ? node->line : 0);
    } else {
        // symbol record owned by the declaration node
        Symbol **slot = node->type == AST_PARAM ? &node->data.parameter.symbol
                                                : &node->data.varDeclaration.symbol;
        free(*slot);
        *slot = owned;
        ++state->nextId;
    }
}

static bool isTerm(const Node *node) {
    if (!node) {
        return false;
    }
    if (node->type == AST_IDENTIFIER || node->type == AST_INT || node->type == AST_DOUBLE ||
        node->type == AST_STRING || node->type == AST_UNIT) {
        return true;
    }
    return node->type == AST_UNARY && node->data.unary.operand &&
           (node->data.unary.op == TOKEN_MINUS || node->data.unary.op == TOKEN_PLUS) &&
           (node->data.unary.operand->type == AST_INT ||
            node->data.unary.operand->type == AST_DOUBLE);
}

static DeclType checkCall(AnalysisState *state, Error *error, Node *node) {
    const char *name = node->data.call.id;
    if (!name) {
        handleError(error, INTERNAL_ERROR, "Function call has no name.", node ? node->line : 0);
        return TYPE_UNSPECIFIED;
    }
    Symbol symbol;
    if (findVariable(state, name, &symbol)) {
        handleError(error, SEM_ERROR_UNDEFINED, "Local name does not refer to a callable function.", node ? node->line : 0);
        return TYPE_UNSPECIFIED;
    }
    Node *function = NULL;
    FunctionInfo *info = NULL;
    const Builtin *builtin = NULL;
    // look up user functions before built-ins
    if (symtableFind(state->functions, name, &symbol)) {
        node->data.call.symbol = symbol.declaration->data.function.symbol;
        function = symbol.declaration;
        info = &state->functionInfo[symbol.id - 1];
    } else {
        for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); ++i) {
            if (strcmp(name, builtins[i].name) == 0) {
                builtin = &builtins[i];
                break;
            }
        }
    }
    if (!function && !builtin) {
        handleError(error, SEM_ERROR_UNDEFINED, "Undefined function.", node ? node->line : 0);
        return TYPE_UNSPECIFIED;
    }
    size_t count = function ? function->data.function.params.size : builtin->count;
    if (node->data.call.args.size != count) {
        handleError(error, SEM_ERROR_INVALID, "Invalid number of arguments.", node ? node->line : 0);
        return TYPE_UNSPECIFIED;
    }
    // check argument form and expected parameter types
    ListNode *argument = node->data.call.args.first;
    ListNode *parameter = function ? function->data.function.params.first : NULL;
    for (size_t i = 0; i < count && error->code == SUCCESS; ++i) {
        Node *arg = *(Node **)argument->data;
        if (!isTerm(arg)) {
            handleError(error, SYN_ERROR, "Arguments must be terms without the FUNEXP extension.", arg ? arg->line : 0);
            return TYPE_UNSPECIFIED;
        }
        DeclType actual = checkExpression(state, error, arg, false, false);
        DeclType expected =
            function ? (*(Node **)parameter->data)->data.parameter.declType : builtin->params[i];
        if (!compatible(arg, actual, expected)) {
            handleError(error, SEM_ERROR_INVALID, "Invalid argument type.", arg ? arg->line : 0);
        }
        argument = argument->next;
        if (parameter)
            parameter = parameter->next;
    }
    if (error->code != SUCCESS) {
        return TYPE_UNSPECIFIED;
    }
    if (!function) {
        return builtin->result;
    }
    // require a declared return type for recursive calls
    if (info->state == FUNCTION_CHECKING) {
        if (function->data.function.returnType == TYPE_UNSPECIFIED) {
            handleError(error, SEM_ERROR_NOMATCH, "Recursion requires an explicit return type.", function ? function->line : 0);
        }
        if (info->index < state->current->lowLink) {
            state->current->lowLink = info->index;
        }
        return function->data.function.returnType;
    }
    bool fresh = info->state == FUNCTION_NEW;
    checkFunction(state, error, info);
    if (info->onStack) {
        size_t lowLink = fresh ? info->lowLink : info->index;
        if (lowLink < state->current->lowLink) {
            state->current->lowLink = lowLink;
        }
    }
    return function->valueType;
}

static DeclType checkBinary(AnalysisState *state, Error *error, Node *node) {
    Node *left = node->data.binary.left;
    Node *right = node->data.binary.right;
    DeclType leftType = checkExpression(state, error, left, false, false);
    if (error->code != SUCCESS) {
        return TYPE_UNSPECIFIED;
    }
    DeclType rightType = checkExpression(state, error, right, false, false);
    if (error->code != SUCCESS) {
        return TYPE_UNSPECIFIED;
    }
    TokenType op = node->data.binary.op;
    bool numbers = isNumeric(leftType) && isNumeric(rightType);
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
            return (leftType == TYPE_DOUBLE || rightType == TYPE_DOUBLE) ? TYPE_DOUBLE : TYPE_INT;
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
        handleError(error, INTERNAL_ERROR, "Unknown operator in AST.", node ? node->line : 0);
        return TYPE_UNSPECIFIED;
    }
    handleError(error, SEM_ERROR_NOMATCH, "Incompatible operand types.", node ? node->line : 0);
    return TYPE_UNSPECIFIED;
}

static DeclType checkExpression(AnalysisState *state, Error *error, Node *node, bool allowCall, bool inCondition) {
    if (error->code != SUCCESS) {
        return TYPE_UNSPECIFIED;
    }
    if (!node) {
        handleError(error, INTERNAL_ERROR, "Invalid expression children in AST.", node ? node->line : 0);
        return TYPE_UNSPECIFIED;
    }
    node->convertToDouble = false;
    DeclType type = TYPE_UNSPECIFIED;
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
            handleError(error, INTERNAL_ERROR, "Identifier has no name.", node ? node->line : 0);
        } else if (!findVariable(state, node->data.identifier.id, &symbol)) {
            handleError(error, SEM_ERROR_UNDEFINED, "Undefined variable.", node ? node->line : 0);
        } else {
            type = symbol.type;
        }
        break;
    case AST_CALL:
        if (!allowCall) {
            handleError(error, SYN_ERROR, "Calls inside expressions require the FUNEXP extension.", node ? node->line : 0);
        } else {
            type = checkCall(state, error, node);
        }
        break;
    case AST_BINARY:
        type = checkBinary(state, error, node);
        break;
    case AST_UNARY:
        if (!node->data.unary.operand) {
            handleError(error, INTERNAL_ERROR, "Invalid unary expression in AST.", node ? node->line : 0);
            break;
        }
        type = checkExpression(state, error, node->data.unary.operand, false, false);
        if ((node->data.unary.op != TOKEN_MINUS && node->data.unary.op != TOKEN_PLUS) ||
            !isNumeric(type)) {
            handleError(error, SEM_ERROR_NOMATCH, "Unary operator requires a numeric operand.", node ? node->line : 0);
        }
        break;
    default:
        handleError(error, SYN_ERROR, "Expected an expression.", node ? node->line : 0);
        break;
    }
    if (type == TYPE_BOOL && !inCondition) {
        handleError(error, SEM_ERROR_NOMATCH, "Boolean values are only allowed in conditions.", node ? node->line : 0);
    }
    node->valueType = type;
    return type;
}

static void checkCondition(AnalysisState *state, Error *error, Node *node) {
    if (checkExpression(state, error, node, false, true) != TYPE_BOOL) {
        handleError(error, SEM_ERROR_NOMATCH, "Condition must have type Boolean.", node ? node->line : 0);
    }
}

static Flow checkReturn(AnalysisState *state, Error *error, Node *node) {
    Flow flow = {TYPE_UNIT, true, true};
    DeclType expected = state->function->data.function.returnType;
    if (expected == TYPE_UNSPECIFIED) {
        handleError(error, SEM_ERROR_NOMATCH, "Return requires an explicit function return type.", node ? node->line : 0);
        return flow;
    }
    if (!node->data.returnNode.value) {
        if (expected != TYPE_UNIT) {
            handleError(error, SEM_ERROR_NOMATCH, "Return without a value requires Unit.", node ? node->line : 0);
        }
    } else {
        Node *value = node->data.returnNode.value;
        DeclType actual = checkExpression(state, error, value, true, false);
        if (expected != TYPE_UNIT && !compatible(value, actual, expected)) {
            handleError(error, SEM_ERROR_NOMATCH, "Incompatible return type.", node ? node->line : 0);
        }
    }
    return flow;
}

static Flow checkStatement(AnalysisState *state, Error *error, Node *node) {
    Flow flow = {TYPE_UNIT, false, true};
    if (error->code != SUCCESS) {
        return flow;
    }
    if (!node) {
        handleError(error, INTERNAL_ERROR, "Invalid statement children in AST.", node ? node->line : 0);
        return flow;
    }
    node->convertToDouble = false;
    Symbol symbol;
    switch (node->type) {
    case AST_VARDECL: {
        const char *name = node->data.varDeclaration.id;
        if (!name) {
            handleError(error, INTERNAL_ERROR, "Declaration has no name.", node ? node->line : 0);
            break;
        }
        if (symtableFind(state->scope->table, name, &symbol)) {
            handleError(error, SEM_ERROR_REDEFINED, "Duplicate definition in the same block.", node ? node->line : 0);
            break;
        }
        DeclType expected = node->data.varDeclaration.declType;
        if (expected != TYPE_UNSPECIFIED && !isUserType(expected)) {
            handleError(error, SEM_ERROR_UNDEFINED, "Invalid declared type.", node ? node->line : 0);
            break;
        }
        // check the initializer before adding the new variable
        Node *value = node->data.varDeclaration.initializer;
        DeclType actual = checkExpression(state, error, value, true, false);
        if (expected == TYPE_UNSPECIFIED) {
            expected = actual;
        }
        if (!compatible(value, actual, expected)) {
            handleError(error, SEM_ERROR_NOMATCH, "Incompatible initializer type.", node ? node->line : 0);
        }
        if (error->code == SUCCESS) {
            defineVariable(state, error, node, name, expected, node->data.varDeclaration.mutable);
        }

        // store the resolved variable type in the node
        node->data.varDeclaration.declType = expected;
        return flow;
    }
    case AST_ASSIGN: {
        if (!node->data.assignment.name) {
            handleError(error, INTERNAL_ERROR, "Assignment has no target name.", node ? node->line : 0);
            break;
        }
        if (!findVariable(state, node->data.assignment.name, &symbol)) {
            handleError(error, SEM_ERROR_UNDEFINED, "Assignment to an undefined variable.", node ? node->line : 0);
            break;
        }
        if (!symbol.mutable) {
            handleError(error, SEM_ERROR_VAL, "Cannot assign to a val or parameter.", node ? node->line : 0);
            break;
        }
        Node *value = node->data.assignment.value;
        DeclType actual = checkExpression(state, error, value, true, false);
        if (!compatible(value, actual, symbol.type)) {
            handleError(error, SEM_ERROR_NOMATCH, "Incompatible assignment type.", node ? node->line : 0);
        }
        break;
    }
    case AST_RETURN:
        flow = checkReturn(state, error, node);
        break;
    case AST_IF: {
        if (!node->data.ifNode.cond || !node->data.ifNode.branchThen ||
            !node->data.ifNode.branchElse) {
            handleError(error, SYN_ERROR, "An if statement requires both branches in the basic language.", node ? node->line : 0);
            break;
        }
        checkCondition(state, error, node->data.ifNode.cond);
        if (error->code != SUCCESS) {
            break;
        }
        // combine return information from both branches
        Flow yes = checkBlock(state, error, node->data.ifNode.branchThen, true);
        Flow no = checkBlock(state, error, node->data.ifNode.branchElse, true);
        flow.returns = yes.returns && no.returns;
        flow.unitTail =
            yes.unitTail && no.unitTail && yes.type == TYPE_UNIT && no.type == TYPE_UNIT;
        break;
    }
    case AST_WHILE:
        if (!node->data.whileNode.cond || !node->data.whileNode.body) {
            handleError(error, INTERNAL_ERROR, "Invalid while statement in AST.", node ? node->line : 0);
            break;
        }
        checkCondition(state, error, node->data.whileNode.cond);
        if (error->code != SUCCESS) {
            break;
        }
        // loop body may never run, so return is not guaranteed
        (void)checkBlock(state, error, node->data.whileNode.body, true);

        break;
    case AST_BLOCK:
        handleError(error, SYN_ERROR, "Standalone nested blocks are not supported in basic IFJ26.", node ? node->line : 0);
        break;
    default:
        flow.type = checkExpression(state, error, node, true, false);
        flow.unitTail = flow.type == TYPE_UNIT;
        return flow;
    }
    node->valueType = flow.type;
    return flow;
}

static Flow checkBlock(AnalysisState *state, Error *error, Node *node, bool newScope) {
    Flow flow = {TYPE_UNIT, false, true};
    if (error->code != SUCCESS) {
        return flow;
    }
    if (!node || node->type != AST_BLOCK || !node->data.block.statements.size) {
        handleError(error, SYN_ERROR, "Expected a nonempty block.", node ? node->line : 0);
        return flow;
    }
    Scope *savedScope = state->scope;
    Scope scope;
    if (newScope) {
        if (!initScope(state, error, &scope, node)) {
            return flow;
        }
        state->scope = &scope;
    }
    for (ListNode *item = node->data.block.statements.first; item && error->code == SUCCESS;
         item = item->next) {
        flow = checkStatement(state, error, *(Node **)item->data);
    }
    // store the block result and restore the outer scope
    node->valueType = flow.type;
    state->scope = savedScope;
    if (newScope) {
        symtableDestroy(scope.table);
    }
    return flow;
}

static void checkFunctionResult(Error *error, Node *function, Node *body, Flow result) {
    if (error->code != SUCCESS) {
        return;
    }
    DeclType expected = function->data.function.returnType;
    Node *last = body;
    if (body->type == AST_BLOCK && body->data.block.statements.last) {
        last = *(Node **)body->data.block.statements.last->data;
    }
    if (!result.returns) {
        if (last->type == AST_IF && expected != TYPE_UNIT &&
            !(expected == TYPE_UNSPECIFIED && result.unitTail)) {
            handleError(error, SEM_ERROR_NOMATCH, "Invalid trailing if statement in function body.", last ? last->line : 0);
        } else if (expected == TYPE_UNSPECIFIED) {
            expected = result.type;
        } else if (expected != TYPE_UNIT && !compatible(last, result.type, expected)) {
            handleError(error, SEM_ERROR_NOMATCH, "Incompatible function body result.", last ? last->line : 0);
        }
    }
    function->valueType = expected;
    if (function->data.function.symbol)
        function->data.function.symbol->type = expected;
    if (error->code == SUCCESS && function->data.function.isMain && expected != TYPE_UNIT) {
        handleError(error, SEM_ERROR_NOMATCH, "Entry function must return Unit.", function ? function->line : 0);
    }
}

static void checkRecursion(AnalysisState *state, Error *error, FunctionInfo *info) {
    if (info->lowLink != info->index) {
        return;
    }
    // remove the completed function group from the stack
    size_t end = state->componentCount;
    FunctionInfo *item;
    do {
        item = state->componentStack[--state->componentCount];
        item->onStack = false;
    } while (item != info);
    // require explicit return types in a recursive group
    if (end - state->componentCount > 1) {
        for (size_t i = state->componentCount; i < end; ++i) {
            item = state->componentStack[i];
            if (item->node->data.function.returnType == TYPE_UNSPECIFIED) {
                handleError(error, SEM_ERROR_NOMATCH, "Recursion requires an explicit return type.", item->node ? item->node->line : 0);
            }
        }
    }
}

static void checkFunction(AnalysisState *state, Error *error, FunctionInfo *info) {
    Node *function = info->node;
    if (error->code != SUCCESS || info->state == FUNCTION_CHECKED) {
        return;
    }
    info->state = FUNCTION_CHECKING;
    info->index = state->nextIndex++;
    info->lowLink = info->index;
    info->onStack = true;
    state->componentStack[state->componentCount++] = info;
    // save the caller scope and current function
    Scope *savedScope = state->scope;
    Node *savedFunction = state->function;
    FunctionInfo *savedInfo = state->current;
    state->scope = NULL;
    state->function = function;
    state->current = info;
    Scope locals;
    if (initScope(state, error, &locals, function)) {
        state->scope = &locals;
        for (ListNode *item = function->data.function.params.first;
             item && error->code == SUCCESS; item = item->next) {
            Node *param = *(Node **)item->data;
            param->valueType = param->data.parameter.declType;
            defineVariable(state, error, param, param->data.parameter.id, param->valueType, false);
        }
        Node *body = function->data.function.body;
        Flow result;
        if (body->type == AST_BLOCK) {
            result = checkBlock(state, error, body, false);
        } else {
            DeclType type = checkExpression(state, error, body, true, false);
            result = (Flow){type, false, type == TYPE_UNIT};
        }
        checkFunctionResult(error, function, body, result);
        symtableDestroy(locals.table);
    }
    // restore the caller state after checking the function
    state->scope = savedScope;
    state->function = savedFunction;
    state->current = savedInfo;
    info->state = FUNCTION_CHECKED;
    checkRecursion(state, error, info);
}

static void firstPass(AnalysisState *state, Error *error, Node *program) {
    size_t mainCount = 0;
    size_t i = 0;
    for (ListNode *item = program->data.program.declarations.first;
         item && error->code == SUCCESS; item = item->next, ++i) {
        Node *fn = *(Node **)item->data;
        if (!fn || fn->type != AST_FUNCTION || !fn->data.function.id || !fn->data.function.body) {
            handleError(error, INTERNAL_ERROR, "Invalid function in AST.", fn ? fn->line : 0);
            break;
        }
        state->functionInfo[i].node = fn;
        fn->valueType = TYPE_UNSPECIFIED;
        fn->convertToDouble = false;
        DeclType type = fn->data.function.returnType;
        if (type != TYPE_UNSPECIFIED && !isUserType(type)) {
            handleError(error, SEM_ERROR_UNDEFINED, "Invalid return type.", fn ? fn->line : 0);
            break;
        }
        for (ListNode *paramItem = fn->data.function.params.first;
             paramItem && error->code == SUCCESS; paramItem = paramItem->next) {
            Node *param = *(Node **)paramItem->data;
            if (!param || param->type != AST_PARAM || !param->data.parameter.id) {
                handleError(error, INTERNAL_ERROR, "Invalid parameter in AST.", fn ? fn->line : 0);
                break;
            }
            if (!isUserType(param->data.parameter.declType)) {
                handleError(error, SEM_ERROR_UNDEFINED, "Invalid parameter type.", param ? param->line : 0);
            }
        }
        if (error->code != SUCCESS) {
            break;
        }
        Symbol symbol = {SYMBOL_FUNCTION, type, false, i + 1, fn};
        Symbol *owned = malloc(sizeof(*owned));
        if (!owned) {
            handleError(error, INTERNAL_ERROR, "Cannot allocate function symbol.", fn ? fn->line : 0);
            break;
        }
        *owned = symbol;
        free(fn->data.function.symbol);
        fn->data.function.symbol = owned;
        ErrorCode code = symtableInsert(state->functions, fn->data.function.id, symbol);
        if (code != SUCCESS) {
            handleError(error, code, "Duplicate function or symbol table error.", fn ? fn->line : 0);
        }
        if (fn->data.function.isMain) {
            ++mainCount;
            if (mainCount > 1) {
                handleError(error, SEM_ERROR_REDEFINED, "Multiple entry functions.", fn ? fn->line : 0);
            } else if (fn->data.function.params.size != 0) {
                handleError(error, SEM_ERROR_INVALID, "Entry function must not have parameters.", fn ? fn->line : 0);
            } else if (type != TYPE_UNSPECIFIED && type != TYPE_UNIT) {
                handleError(error, SEM_ERROR_NOMATCH, "Entry function must return Unit.", fn ? fn->line : 0);
            }
        }
    }
    if (!mainCount) {
        handleError(error, SEM_ERROR_UNDEFINED, "Missing entry function.", program ? program->line : 0);
    }
}

static void secondPass(AnalysisState *state, Error *error, Node *program) {
    for (size_t i = 0; i < program->data.program.declarations.size && error->code == SUCCESS;
         ++i) {
        checkFunction(state, error, &state->functionInfo[i]);
    }
}

ErrorCode semanticsRun(Node *program, SemanticError *err) {
    AnalysisState state = {0};
    state.nextId = 1;
    Error error = {.code = SUCCESS, .message = "OK", .line = 0};

    if (!program || program->type != AST_PROGRAM ||
        (program->data.program.declarations.size > 0 &&
         !program->data.program.declarations.first)) {
        handleError(&error, INTERNAL_ERROR, "Expected an AST_PROGRAM root.", program ? program->line : 0);
    }

    if (error.code == SUCCESS) {
        state.functions = symtableCreate();
        if (!state.functions) {
            handleError(&error, INTERNAL_ERROR, "Cannot create function table.", program ? program->line : 0);
        }
    }

    if (error.code == SUCCESS) {
        size_t count = program->data.program.declarations.size;
        if (count == 0) {
            count = 1;
        }
        state.componentStack = calloc(count, sizeof(*state.componentStack));
        state.functionInfo = calloc(count, sizeof(*state.functionInfo));
        if (!state.componentStack || !state.functionInfo) {
            handleError(&error, INTERNAL_ERROR, "Cannot allocate function analysis state.", program ? program->line : 0);
        }
    }

    // first pass: register functions and check their headers
    if (error.code == SUCCESS) {
        firstPass(&state, &error, program);
    }
    // second pass: check function bodies and return values
    if (error.code == SUCCESS) {
        secondPass(&state, &error, program);
    }

    // release temporary tables and function tracking data
    free(state.componentStack);
    free(state.functionInfo);
    symtableDestroy(state.functions);

    if (err) {
        err->code = error.code;
        err->line = error.line;
        err->message = error.message;
    }
    return error.code;
}
