/**
 * @file syn.c
 * @brief Syntactical analysis
 *
 * Parses token stream (using top to bottom recursive descent)
 * and then creates AST nodes.
 *
 * @authors
 * - Bára Wilhelmová (xwilheb00)
 *
 */


#include "init.h"

/* ------------------------ STACK OPERATIONS --------------------- */
// stack initialization
void initStack(Stack *stack) {
    stack->el = NULL;
    stack->top = 0;
    stack->capacity = 0;
}

/**
 * @brief Inserts element on top of initialized stack.
 *
 * @param stack
 * @param el element we want to push to stack
 * @return true on success else false
 */
bool pushStack(Stack *stack, StackEl el) {
    if (stack->top >= stack->capacity) {
        size_t newCap = stack->capacity;

        if (newCap == 0) {
            newCap = 8; // default size
        } else {
            newCap *= 2; // double existing capacity
        }

        StackEl *tmp = realloc(stack->el, newCap * sizeof(el));
        if (!tmp) return false;

        stack->el = tmp;
        stack->capacity = newCap;
    }
    stack->el[stack->top] = el;
    stack->top++;
    return true;
}

/**
 * @brief Pops element from top of stack.
 *
 * @param stack
 * @return true on success else false
 */
bool popStack(Stack *stack) {
    if (stack->top > 0) {
        stack->top--;
        return true;
    }
    return false;
}

/**
 * @brief Returns the element on top of nonempty stack
 *
 * @param stack
 * @return element else NULL
 */
StackEl *topStack(Stack *stack) {
    if (stack->top == 0) {
        // parser musi zavolat error!!!
        return NULL;
    }
    return &stack->el[stack->top - 1];
}

/**
 * @brief Inserts a border next to the terminal that's closest to the top of the stack.
 *
 * @param stack
 * @param termIdx index of the closest terminal
 * @param line
 * @return true on success else false
 */
bool insertBorder(Stack *stack, size_t termIdx, int line) {
    StackEl border = {.symbol = BORDER, .node = NULL, .line = line};

    if (!pushStack(stack, border)) return false;
    size_t insertIdx = termIdx + 1;

    // moving elements to the right so we do not overwrite the el that we have to move
    for (size_t i = stack->top - 1; i > insertIdx; i--) {
        stack->el[i] = stack->el[i - 1];
    }
    stack->el[insertIdx] = border;
    return true;
}

/* ============================ PARSER =============================== */
/**
 * @brief Free() for token_id and string (needed because of strdup allocation in lex).
 *
 * @param token token to be destroyed
 */
void freeToken(Token *token) {
    if (!token) return;

    switch (token->type) {
        // only for tokens that use strdup() in LEX
        case TOKEN_ID:
        case TOKEN_STRING_LITERAL:
            free(token->stringValue);
            break;
        default:
            break;
    }
    token->type = TOKEN_EOF; // lowers the risk of double free
    token->stringValue = NULL;
}

/**
 * @brief Initializes parser with default values.
 *
 * @param parser
 */
void parserInit(Parser *parser) {
    parser->error.code =  SUCCESS;
    parser->error.message = "";
    parser->error.line = 0;
    parser->fortuneTell = false;

    // compound literal that uses designated initializer -> creates temporary object Token whose value we assign to prev
    parser->previousToken = (Token){.type = TOKEN_EOF}; // we need to initialize it so we can safely call freeToken
    parser->currentToken = nextToken();
}

// syn cleanup - lex transfers ownership -> syn has to free tokens
void synClean(Parser *parser) {
    freeToken(&parser->previousToken);
    freeToken(&parser->currentToken);
}

/**
 * @brief Moves from current token onto the next by calling nextToken().
 *
 * @param parser
 */
bool parseNext(Parser *parser){
    freeToken(&parser->previousToken);

    parser->previousToken = parser->currentToken;
    if (parser->fortuneTell != true) {
        parser->currentToken = nextToken(&parser->error);
        return true;
    }

    parser->currentToken = parser->fortune;
    parser->fortuneTell = false;

    return false;
}

// parser utils
/**
 * @brief Compares current token type with the desired type.
 *
 * @param parser
 * @param type desired token type
 * @return true on success else false
 */
bool checkType(Parser *parser, TokenType type) {
    return parser->currentToken.type == type;
}

/**
 * @brief Compares token type and moves onto the next.
 *
 * @param parser
 * @param type
 * @return true on success else false
 */
bool same(Parser *parser, TokenType type) {
    if (!checkType(parser, type)) return false;

    parseNext(parser);
    return true;
}

// wanted token
/**
 * @brief Checks type and moves onto the next, if this fails handles error.
 *
 * @param parser
 * @param type
 * @param message
 * @return true on success else false (+ error)
 */
bool want(Parser *parser, TokenType type, const char *message) {
    if(checkType(parser, type)){
        parseNext(parser);
        return true;
    }

    handleError(&parser->error, SYN_ERROR, message, parser->currentToken.lineNumber);
    return false;
}

// for handing string value copies to ast
char *copyStr(const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str) + 1;
    char *copy = malloc(len);

    if (!copy) return NULL;
    memcpy(copy, str, len);
    return copy;
}

// MINI AST
// PROLOG - je povinny takze staci bool neni treba pro nej vytvaret node
/**
 * @brief Checks the correctness of prolog.
 *
 * @param parser
 * @return true on success else false
 */
bool parseProlog(Parser *parser) {

    if(!want(parser, TOKEN_KW_IMPORT, "Invalid prolog: Expected 'import'")){
        return false;
    }

    if(!want(parser, TOKEN_ID, "Invalid prolog: Expected 'ifj26'")){
        return false;
    }

    if(!want(parser, TOKEN_DOT, "Invalid prolog: Expected '.'")){
        return false;
    }

	if(!want(parser, TOKEN_MUL, "Invalid prolog: Expected '*'")){
        return false;
	}

    if(!want(parser, TOKEN_EOL, "Expected end of prolog line")){
        return false;
    }

    return true;
}

// PROGRAM
/**
 * @brief Parses prolog, functions and checks EOF.
 *
 * @param parser
 * @return node of program on success else NULL
 */
Node *parseProgram(Parser *parser) {
    int line = parser->currentToken.lineNumber;
    if(!parseProlog(parser)) return NULL;

    Node *program = createAST(AST_PROGRAM, line);
    if (!program) {
        handleError(&parser->error, INTERNAL_ERROR, "Allocation of program node failed", parser->currentToken.lineNumber);
        return NULL;
    }

    Node *fun = parseFunction(parser);
    if (!fun) {
        destroyAST(program);
        return NULL;
    }

    if (!insertFirst(&program->data.program.declarations, &fun)) {
        destroyAST(fun);
        destroyAST(program);
        handleError(&parser->error, INTERNAL_ERROR, "Failed to add function declaration", parser->currentToken.lineNumber);
        return NULL;
    }

    if(!checkType(parser, TOKEN_EOF)) {
        destroyAST(program);
        handleError(&parser->error, SYN_ERROR, "Expected end of file", parser->currentToken.lineNumber);
        return NULL;
    }

    return program; // vraci root node ast
}

// LITERAL (INT, DOUBLE, STRING, UNIT?)
/**
 * @brief Parses literal tokens and creates AST node accordingly.
 *
 * @param parser
 * @return node of literal on success else NULL
 */
Node *parseLiteral(Parser *parser) {
    Token *token = &parser->currentToken;
    NodeType type;

    switch (token->type) {
        case TOKEN_INT_LITERAL:
            type = AST_INT;
            break;
        case TOKEN_DOUBLE_LITERAL:
            type = AST_DOUBLE;
            break;
        case TOKEN_STRING_LITERAL:
            type = AST_STRING;
            break;

        default:
            handleError(&parser->error, SYN_ERROR, "Expected literal", parser->currentToken.lineNumber);
            return NULL;
    }

    Node *literal = createAST(type, token->lineNumber);
    if (!literal) {
        handleError(&parser->error, INTERNAL_ERROR, "Allocation of literal AST node failed", parser->currentToken.lineNumber);
    }

    switch (type) {
        case AST_INT:
            literal->data.intLiteral.value = token->intValue;
            break;

        case AST_DOUBLE:
            literal->data.doubleLiteral.value = token->doubleValue;
            break;

        case AST_STRING:
            literal->data.stringLiteral.string = copyStr(token->stringValue);
            if(!literal->data.stringLiteral.string) {
                handleError(&parser->error, INTERNAL_ERROR, "Allocation of string copy AST node failed", parser->currentToken.lineNumber);
                destroyAST(literal);
                return NULL;
            }
            break;

        default:
            break;
    }
    parseNext(parser);
    return literal;
}

// IDENTIFIER
/**
 * @brief Parses token identifier and creates node for AST.
 *
 * @param parser
 * @return id node on success else NULL
 */
Node *parseId(Parser *parser) {
    if(!checkType(parser, TOKEN_ID)) {
        handleError(&parser->error, SYN_ERROR, "Expected identifier", parser->currentToken.lineNumber);
        return NULL;
    }
    int line = parser->currentToken.lineNumber;

    Node *id = createAST(AST_IDENTIFIER, line);
    if (!id) {
        handleError(&parser->error, INTERNAL_ERROR, "Failed to allocate identifier", parser->currentToken.lineNumber);
        return NULL;
    }

    id->data.identifier.id = copyStr(parser->currentToken.stringValue);
    if (!id->data.identifier.id) {
        handleError(&parser->error, INTERNAL_ERROR, "Failed to make a copy of identifier", parser->currentToken.lineNumber);
        destroyAST(id);
        return NULL;
    }

    parseNext(parser);
    return id;
}

// CALL
/**
 * @brief Parses function call and creates node for AST.
 *
 * @param parser
 * @param id pointer to id node from parseId()
 * @return call node on success else NULL
 */
Node *parseCall(Parser *parser, Node *id) {

    int line = id->line;
    Node *call = createAST(AST_CALL, line);

    if (!call) {
        handleError(&parser->error, INTERNAL_ERROR, "Failed to allocate 'call' node", parser->currentToken.lineNumber);
        return NULL;
    }
    // str ownership transfer (id -> call)
    call->data.call.id = id->data.identifier.id;
    id->data.identifier.id = NULL;
    destroyAST(id);

    if (!call->data.call.id) {
        // error id is null
        handleError(&parser->error, SYN_ERROR, "Identifier is NULL", line);
        destroyAST(call);
        return NULL;
    }

    if (!want(parser, TOKEN_LPAREN, "Expected '('")) {
        destroyAST(call);
        return NULL;
    }

    // if u do not find ) -> expect params
    if (!checkType(parser, TOKEN_RPAREN)) {
        do {
            Node *arg;
            // temporary solution for nested function call
            if (checkType(parser, TOKEN_ID)) {
                arg = parseTerm(parser);
            } else {
                arg = parseLiteral(parser);
            }

            if (!arg) {
                destroyAST(call);
                handleError(&parser->error, SYN_ERROR, "Expected argument", line);
                return NULL;
            }
            // args as list
            List *args = &call->data.call.args;
            bool insertArg;

            if (listIsEmpty(args)) {
                insertArg = insertFirst(args, &arg);
            } else {
                listLast(args);
                insertArg = insertAfter(args, &arg);
            }

            if (!insertArg) {
                destroyAST(arg);
                destroyAST(call);
                handleError(&parser->error, INTERNAL_ERROR, "Failed to add argument", line);
                return false;
            }

            /* old varianta pres children
            if (!addChild(call, arg)) {
                handleError(&parser->error, INTERNAL_ERROR, "Failed to add child node", line);
                destroyAST(arg);
                destroyAST(call);
                return NULL;
            }
            */
        } while (same(parser, TOKEN_COMMA));

    }

    if (!want(parser, TOKEN_RPAREN, "Expected ')'")) {
        destroyAST(call);
        return NULL;
    }

    return call;
}

/* ======================== PRECEDENCE ============================== */
static const char precedenceTable[9][9] = {
    // > ... reduce - neposouvej hranici redukuj vstup nespotrebuj token
    // < ... shift - vloz hranici a vstup
    // ! ... invalid operation combination
    // = ... insert input and shift
    // E ... end state

    // INPUT: neg operand add_sub mul_div lparen rparen comp equal end
    // ON STACK:
    // neg
    {'<', '<', '>', '>', '<', '>', '>', '>', '>'},
    // operand
    {'!', '!', '>', '>', '!', '>', '>', '>', '>'},
    // add_sub
    {'<', '<', '>', '<', '<', '>', '>', '>', '>'},
    // mul_div
    {'<','<', '>', '>', '<', '>', '>', '>', '>'},
    // lparen
    {'<', '<', '<', '<', '<', '=', '<', '<','!'},
    // rparen
    {'!','!', '>', '>', '!', '>', '>', '>', '>'},
    // comp
    {'<', '<', '<', '<', '<', '>', '!', '>', '>'},
    // equal
    {'<', '<', '<', '<', '<', '>', '<', '!', '>'},
    // end
    {'<', '<', '<', '<', '<', '!', '<', '<', 'E'},
};

/**
 * @brief Pushes input onto precedence stack.
 *
 * For action <, inserts BORDER after the terminal closest to the top of the stack. Action = just pushes the element
 * without BORDER. Creates AST node and consumes the current token.
 *
 * @param parser
 * @param stack receives new element
 * @param action '<' and '=' (from precedence table)
 * @param symbol role in the expression
 * @param termIdx index of closest terminal to the stack's top
 * @return true on success, false when border insertion, operand parsing or pushing the element fails
 */
bool shiftStack(Parser *parser, Stack *stack, char action, OpSymbol symbol, size_t termIdx) {
    Token *token = &parser->currentToken;

    if (action == '<') {
        if (!insertBorder(stack, termIdx, parser->currentToken.lineNumber)) {
            handleError(&parser->error, INTERNAL_ERROR, "Failed to insert border", token->lineNumber);
            return false;
        }
    }
    StackEl item = {.symbol = symbol,.op = token->type, .node = NULL, .line = token->lineNumber}; // input el
    if (item.symbol == OPERAND) {
        item.node = parseTerm(parser); // parseLiteral()
        if (!item.node) {
            return false;
        }
    } else {
        parseNext(parser); // for operator/parenthesis -> no ast for them
    }

    if (!pushStack(stack, item)) {
        handleError(&parser->error, INTERNAL_ERROR, "Failed to push element", item.line);
        if (item.node) {
            destroyAST(item.node);
        }
        return false;
    }

    return true;
}

/**
 * @brief Reduces elements after BORDER to nonterminal.
 *
 * Finds the last BORDER based on the rule used creates node of AST for binary operation or reuses one for operands/expression.
 * Replaces BORDER with elements with result expression.
 *
 * @param parser
 * @param stack
 * @return true on succes, false when border is missing, allocation of node fails or it is invalid reduce(no rule matches)
 */
bool reduceStack(Parser *parser, Stack *stack) {
    size_t borderIdx = stack->top;

    while (borderIdx > 0 && stack->el[borderIdx - 1].symbol != BORDER) {
        borderIdx--; // cestujeme dolu zasobnikem
    }

    if (borderIdx == 0) {
        handleError(&parser->error, SYN_ERROR, "Stack is missing border of expression", parser->currentToken.lineNumber);
        return false;
    }

    borderIdx--;

    size_t count = stack->top - borderIdx - 1;
    // one operand only
    if (count == 1 && stack->el[borderIdx + 1].symbol == OPERAND) {
        // reduce
        StackEl result = stack->el[borderIdx + 1];
        result.symbol = EXPR;

        stack->el[borderIdx] = result;
        stack->top = borderIdx + 1;
        return true;
    }

    // () -> unit
    if (count == 2 && stack->el[borderIdx + 1].symbol == LPAREN && stack->el[borderIdx + 2].symbol == RPAREN) {
        Node *unit = createAST(AST_UNIT, stack->el[borderIdx + 1].line); // UNIT basically an empty expr
        if (!unit) {
            handleError(&parser->error, INTERNAL_ERROR, "Failed to allocate 'unit' node", stack->el[borderIdx + 1].line);
            return false;
        }

        stack->el[borderIdx].node = unit;
        stack->el[borderIdx].symbol = EXPR;
        stack->el[borderIdx].line = stack->el[borderIdx + 1].line;

        stack->top = borderIdx + 1;
        return true;
    }

    if (count == 2 && stack->el[borderIdx + 1].symbol == NEG && stack->el[borderIdx + 2].symbol == EXPR) {
        StackEl *sign = &stack->el[borderIdx + 1];
        StackEl *num = &stack->el[borderIdx + 2];

        Node *unary = createAST(AST_UNARY, sign->line);
        if (!unary) {
            handleError(&parser->error, INTERNAL_ERROR, "Failed to allocate 'unary' node", sign->line);
            return false;
        }

        unary->data.unary.op = sign->op;
        unary->data.unary.operand = num->node;

        stack->el[borderIdx].node = unary;
        stack->el[borderIdx].line = sign->line;
        stack->el[borderIdx].symbol = EXPR;

        stack->top = borderIdx + 1;
        return true;
    }

    // expr operator expr
    if (count == 3) {
        StackEl *left = &stack->el[borderIdx + 1];
        StackEl *mid = &stack->el[borderIdx + 2];
        StackEl *right = &stack->el[borderIdx + 3];

        if (left->symbol == EXPR && right->symbol == EXPR &&
            (mid->symbol == ADD_SUB || mid->symbol == MUL_DIV || mid->symbol == COMP || mid->symbol == EQUAL)) {
            // ast here
            Node *bin = createAST(AST_BINARY, mid->line);
            if (!bin) {
                handleError(&parser->error, INTERNAL_ERROR, "Failed to allocate 'binary' node", mid->line);
                return false;
            }
            // operands
            bin->data.binary.left = left->node;
            bin->data.binary.right = right->node;
            // operator
            bin->data.binary.op = mid->op;

            StackEl result = {.symbol = EXPR, .node = bin, .line = mid->line};

            stack->el[borderIdx] = result;
            stack->top = borderIdx + 1;
            return true;
        }

        // (expr) -> expr
        if (left->symbol == LPAREN && mid->symbol ==  EXPR && right->symbol == RPAREN ) {
            StackEl result = *mid;

            stack->el[borderIdx] = result;
            stack->top = borderIdx + 1;
            return true;
        }

    }
    handleError(&parser->error, SYN_ERROR, "Invalid expression reduction", parser->currentToken.lineNumber);
    return false;
}

// EXPRESSION
/**
 * @brief Processes input token into an expression.
 *
 * Inserts expression end (END), based on token type determines the OpSymbol, then finds the adequate cell from precedence
 * table and performs action accordingly.
 *
 * @param parser
 * @return node of AST on success, else NULL
 */
Node *parseExpression(Parser *parser) {
    Token *token = &parser->currentToken;
    int line = token->lineNumber;
    Stack stack;
    OpSymbol symbol;
    StackEl bottom = {.symbol = END, .line = 0};
    int depth = 0;
    bool oprWanted = true; // expecting an operand

    if (parser->error.code != SUCCESS) return NULL;

    initStack(&stack);
    if (!pushStack(&stack, bottom)) {
        handleError(&parser->error, INTERNAL_ERROR, "Failed to push stack", line);
        return NULL;
    }

    while (true) {
        switch (token->type) {
            case TOKEN_PLUS:
            case TOKEN_MINUS:
                symbol = oprWanted ? NEG : ADD_SUB; // neg ma stejnou precedenci jako unarni + a -
                break;
            // todo: lex chybi ! token
            /*
                case TOKEN_NOT:
                symbol = NEG;
                break;
            */
            case TOKEN_MUL:
            case TOKEN_DIV:
                symbol = MUL_DIV;
                break;
            case TOKEN_LT:
            case TOKEN_LTE:
            case TOKEN_GT:
            case TOKEN_GTE:
                symbol = COMP;
                break;
            case TOKEN_EQ:
            case TOKEN_NEQ:
                symbol = EQUAL;
                break;
            case TOKEN_ID:
            case TOKEN_INT_LITERAL:
            case TOKEN_DOUBLE_LITERAL:
            case TOKEN_STRING_LITERAL:
                symbol = OPERAND;
                break;
            case TOKEN_LPAREN:
                symbol = LPAREN;
                break;
            case TOKEN_RPAREN:
                symbol = depth == 0 ? END : RPAREN;
                break;
            case TOKEN_DEDENT:
            case TOKEN_COMMA:
            case TOKEN_KW_DO:
            case TOKEN_KW_THEN:
            case TOKEN_EOL:
            case TOKEN_EOF:
                symbol = END;
                break;

            default:
                // can not resolve token -> error
                handleError(&parser->error, SYN_ERROR, "Invalid token", token->lineNumber);

                // stack cleanup
                for(size_t i = 0; i < stack.top; i++ ) {
                    if (stack.el[i].node) destroyAST(stack.el[i].node);
                }
                free(stack.el);
                return NULL;
        }

        size_t termIdx = stack.top;
        while (termIdx > 0) {
            termIdx--;
            OpSymbol maybe = stack.el[termIdx].symbol;

            if (maybe != EXPR && maybe != BORDER) break;
        }

        OpSymbol sym = stack.el[termIdx].symbol;
        char action = precedenceTable[sym][symbol];

        switch (action) {
            case '<':
            case '=':
                // insert border a zpracuj a uloz jednu polozku
                if (!shiftStack(parser, &stack, action, symbol, termIdx)) {
                    for (size_t i = 0; i < stack.top; i++) {
                        if (stack.el[i].node) destroyAST(stack.el[i].node);
                    }
                    free(stack.el);
                    return NULL;
                }

                oprWanted = symbol != OPERAND && symbol != RPAREN; // diky tomu pozna a - -42 jako bin a druhe jako unarni

                if (symbol == LPAREN) {
                    depth++;
                } else if (symbol == RPAREN) {
                    depth--;
                }
                break;

            case '>':
                //spoj položky ze zasobniku a udelej z nich binary ast
                if (!reduceStack(parser, &stack)) {
                    for (size_t i = 0; i < stack.top; i++) {
                        if (stack.el[i].node) destroyAST(stack.el[i].node);
                    }
                    free(stack.el);
                    return NULL;
                }
                break;
            case 'E':
                //over konec a uklid zasobnik navrat ast
                if (stack.top == 2 && stack.el[0].symbol == END && stack.el[1].symbol == EXPR && stack.el[1].node) {
                    Node *result = stack.el[1].node;

                    free(stack.el);
                    return result;
                }
            default:
                // chyba uklid
                handleError(&parser->error, SYN_ERROR, "Invalid token", token->lineNumber);

                // stack cleanup
                for(size_t i = 0; i < stack.top; i++ ) {
                    if (stack.el[i].node) destroyAST(stack.el[i].node);
                }
                free(stack.el);
                return NULL;
        }
    }
    //Node *expression = createAST(AST_EXPRESSION, token->lineNumber);
}

// ASSIGNMENT
/**
 * @brief Parses assignment.
 *
 * @param parser
 * @param id pointer to existing id node made by parseId()
 * @return assign node on success else NULL
 */
Node *parseAssignment(Parser *parser,Node *id) {
    Token *token = &parser->currentToken;
    int line = token->lineNumber;

    Node *assign = createAST(AST_ASSIGN, line);
    if (!assign) {
        handleError(&parser->error, INTERNAL_ERROR, "Failed to allocate 'assign' node", line);
        destroyAST(id);
        return NULL;
    }

    assign->data.assignment.name = id->data.identifier.id;
    assign->data.assignment.value = NULL;
    id->data.identifier.id = NULL;
    destroyAST(id);

    if (!assign->data.assignment.name) {
        handleError(&parser->error, SYN_ERROR, "Identifier is NULL", line);
        destroyAST(assign);
        return NULL;
    }

    if (!want(parser, TOKEN_ASSIGN, "Expected '='")) {
        destroyAST(assign);
        return NULL;
    }

    Node *value = parseExpression(parser);
    if (!value) {
        destroyAST(assign);
        return NULL;
    }

    assign->data.assignment.value = value;
    return assign;
}

/**
 * @brief Parses terminals.
 *
 * For TOKEN_ID can determine whether it is just id or function call.
 *
 * @param parser
 * @return node on success else NULL
 */
Node *parseTerm(Parser *parser) {
    Token *token = &parser->currentToken;
    //TokenType type;

    switch (token->type) {
        case TOKEN_INT_LITERAL:
        case TOKEN_DOUBLE_LITERAL:
        case TOKEN_STRING_LITERAL:
            return parseLiteral(parser);

        case TOKEN_ID: {
            Node *id = parseId(parser);
            if (!id) {
                handleError(&parser->error, INTERNAL_ERROR, "Failed to allocate identifier node", parser->currentToken.lineNumber);
                return NULL;
            }

            // if next one is ( -> function call
            if (checkType(parser, TOKEN_LPAREN)) {
                return parseCall(parser, id);
            }

            return id;
        }

        default:
            handleError(&parser->error, SYN_ERROR, "Expected expression", parser->currentToken.lineNumber);
            return NULL;
    }

}

// BLOCK
Node *parseBlock(Parser *parser) {
    int line = parser->currentToken.lineNumber;

    if (!want(parser, TOKEN_INDENT, "Expected indentation")) {
        handleError(&parser->error, SYN_ERROR, "Expected indentation", parser->currentToken.lineNumber);
        return NULL;
    }

    Node *block = createAST(AST_BLOCK, line);
    if (!block) return NULL;

    while (!checkType(parser, TOKEN_DEDENT)) {
        if (checkType(parser, TOKEN_EOF)) {
            handleError(&parser->error, SYN_ERROR, "Expected eof", parser->currentToken.lineNumber);
            destroyAST(block);
            return NULL;
        }

        Node *statement = parseStatement(parser);
        if (!statement) {
            destroyAST(block);
            return NULL;
        }
        // todo: prevazat na list
        List *statements = &block->data.block.statements;
        bool insertStatement;

        if (listIsEmpty(statements)) {
            insertStatement = insertFirst(statements, &statement);
        } else {
            listLast(statements);
            insertStatement = insertAfter(statements, &statement);
        }

        if (!insertStatement) {
            destroyAST(statement);
            return NULL;
        }
    }
    parseNext(parser);
    return block;
}


// FUNCTION
Node *parseFunction(Parser *parser){

    bool main = false;
    char *id = NULL;

    if (same(parser, TOKEN_ANNOTATION_MAIN)){
        main = true;
        // id = "main";
    }

    if (!want(parser, TOKEN_KW_DEF, "Expected 'def'")){
        return NULL;
    }

    if (!checkType(parser, TOKEN_ID)){
        handleError(&parser->error, SYN_ERROR, "Expected identifier", parser->currentToken.lineNumber);
        return NULL;
    }

    Node *fun = createAST(AST_FUNCTION, parser->currentToken.lineNumber);
    if (!fun) {
        handleError(&parser->error, INTERNAL_ERROR, "Failed to allocate 'fun'", parser->currentToken.lineNumber);
        return NULL;
    }

    fun->data.function.main = main;
    fun->data.function.returnType = TYPE_UNSPECIFIED;
    fun->data.function.id = copyStr(parser->currentToken.stringValue);

    if (!fun->data.function.id) {
        destroyAST(fun);
        handleError(&parser->error, INTERNAL_ERROR, "Failed to copy 'fun' id", parser->currentToken.lineNumber);
        return NULL;
    }
    parseNext(parser);

    if(!want(parser, TOKEN_LPAREN, "Expected '('") || !parseParams(parser, &fun->data.function.params) || !want(parser, TOKEN_RPAREN, "Expected ')'")){
        destroyAST(fun);
        return NULL;
    }

    if (same(parser, TOKEN_COLON) && !parseType(parser, &fun->data.function.returnType)) {
        destroyAST(fun);
        return NULL;
    }

    if (!want(parser, TOKEN_ASSIGN, "Expected '='")) {
        destroyAST(fun);
        return NULL;
    }

    if (same(parser, TOKEN_EOL)) {
        fun->data.function.body = parseBlock(parser); // block function definition -> block
    } else {
        fun->data.function.body = parseExpression(parser); // inline definition -> expr nb call
        want(parser, TOKEN_EOL, "Expected EOL after function body");
    }

    if (!fun->data.function.body) {
        destroyAST(fun);
        return NULL;
    }

    return fun;
}

// PARAMS
bool parseParams(Parser *parser, List *params) {
    if (checkType(parser, TOKEN_RPAREN)) return true;

    while (true) {
        if (!checkType(parser, TOKEN_ID)) {
            handleError(&parser->error, SYN_ERROR, "Expected param identifier", parser->currentToken.lineNumber);
            return false;
        }

        Node *param = createAST(AST_PARAM, parser->currentToken.lineNumber);
        if (!param) return false;

        param->data.parameter.symbol = NULL;
        param->data.parameter.id = copyStr(parser->currentToken.stringValue);

        if (!param->data.parameter.id) {
            destroyAST(param);
            return false;
        }
        parseNext(parser);
        if (parser->error.code != SUCCESS) {
            destroyAST(param);
            return false;
        }

        if (!want(parser, TOKEN_COLON, "Expected ':'") || !parseType(parser, &param->data.parameter.declType)) {
            destroyAST(param);
            return false;
        }

        // list z params
        bool insertParam;

        if (listIsEmpty(params)) {
            insertParam = insertFirst(params, &param);
        } else {
            listLast(params);
            insertParam = insertAfter(params, &param);
        }

        if (!insertParam) {
            destroyAST(param);
            return false;
        }

        if (checkType(parser, TOKEN_RPAREN)) return true;

        if (!want(parser, TOKEN_COMMA, "Expected ',' or ')'")) {
            return false;
        }
    }
}

// data type
bool parseType(Parser *parser, DeclType *type) {
    DeclType dtype = TYPE_UNSPECIFIED;

    switch (parser->currentToken.type) {
        case TOKEN_TYPE_INT:
            dtype = TYPE_INT;
            break;
        case TOKEN_TYPE_DOUBLE:
            dtype = TYPE_DOUBLE;
            break;
        case TOKEN_TYPE_STRING:
            dtype = TYPE_STRING;
            break;
        case TOKEN_TYPE_UNIT:
            dtype = TYPE_UNIT;
            break;

        default:
            handleError(&parser->error, SYN_ERROR, "Expected decl data type", parser->currentToken.lineNumber);
            return false;
            break;
    }

    parseNext(parser);

    if (parser->error.code != SUCCESS) return false;

    *type = dtype;
    return true;
}

// VAR DECLARATION
Node *parseDecl(Parser *parser) {
    bool changes = checkType(parser, TOKEN_KW_VAR); // 1 -> mutable var, 0 -> nonmutable val

    /*
    if (checkType(parser, TOKEN_KW_VAL)) {
        changes = false;
    } else if (checkType(parser, TOKEN_KW_VAR)) {
        changes = true;
    }
    */
    parseNext(parser);

    if (!checkType(parser, TOKEN_ID)) {
        handleError(&parser->error, SYN_ERROR, "Expected identifier", parser->currentToken.lineNumber);
        return NULL;
    }

    Node *decl = createAST(AST_VARDECL, parser->currentToken.lineNumber);
    if (!decl) {
        handleError(&parser->error, INTERNAL_ERROR, "Failed to allocate var decl node", parser->currentToken.lineNumber);
        return NULL;
    }

    decl->data.varDeclaration.id = copyStr(parser->currentToken.val.stringValue);
    if (!decl->data.varDeclaration.id) {
        destroyAST(decl);
        handleError(&parser->error, SYN_ERROR, "Failed to copy variable id", parser->currentToken.lineNumber);
    }
    decl->data.varDeclaration.mutable = changes;
    decl->data.varDeclaration.declType = TYPE_UNSPECIFIED;
    decl->data.varDeclaration.initializer = NULL;

    parseNext(parser);

    // : means that we expect declared type, otherwise sem must recognize type based on initializer value
    if (same(parser, TOKEN_COLON) && !parseType(parser, &decl->data.varDeclaration.declType)) {
        destroyAST(decl);
        return NULL;
    }

    if (!want(parser, TOKEN_ASSIGN, "Expected '='")) {
        destroyAST(decl);
        return NULL;
    }

    decl->data.varDeclaration.initializer = parseExpression(parser);
    if (!decl->data.varDeclaration.initializer) {
        destroyAST(decl);
        return NULL;
    }
    return decl;
}

// RETURN
Node *parseReturn(Parser *parser) {
    int line = parser->currentToken.lineNumber;
    parseNext(parser); // consumes return kw

    Node *node = createAST(AST_RETURN, line);
    if (!node) {
        handleError(&parser->error, INTERNAL_ERROR, "Failed to allocate return node", line);
        return NULL;
    }

    node->data.returnNode.value = NULL;

    if (!checkType(parser, TOKEN_EOL)) {
        node->data.returnNode.value = parseExpression(parser);

        if (!node->data.returnNode.value) {
            destroyAST(node);
            return NULL;
        }
    }

    return node;
}

// WHILE
Node *parseWhile(Parser *parser) {
    parseNext(parser); // consumes while kw

    Node *cond = parseExpression(parser);
    if (!cond) return NULL;

    if (!want(parser, TOKEN_KW_DO, "Expected do after while") || !want(parser, TOKEN_EOL, "Expected EOL after do")) {
        destroyAST(cond);
        return NULL;
    }

    Node *body = parseBlock(parser);
    if (!body) {
        destroyAST(cond);
        return NULL;
    }

    Node *cycle = createAST(AST_WHILE, parser->currentToken.lineNumber);
    if (!cycle) {
        destroyAST(cond);
        destroyAST(body);
        handleError(&parser->error, INTERNAL_ERROR, "Failed to allocate while loop", parser->currentToken.lineNumber);
        return NULL;
    }

    cycle->data.whileNode.cond = cond;
    cycle->data.whileNode.body = body;
    return cycle;
}

// IF
Node *parseIf(Parser *parser) {
    int line = parser->currentToken.lineNumber;
    parseNext(parser); // consumes if kw

    Node *cond = parseExpression(parser);
    if (!cond) return NULL;

    if (!want(parser, TOKEN_KW_THEN, "Expected then after if")) {
        destroyAST(cond);
        return NULL;
    }

    if (!want(parser, TOKEN_EOL, "Expected EOL after if then")) {
        destroyAST(cond);
        return NULL;
    }

    Node *then = parseBlock(parser);
    if (!then) {
        destroyAST(cond);
        return NULL;
    }

    if (!want(parser, TOKEN_KW_ELSE, "Expected else after if then") || !want(parser, TOKEN_EOL, "Expected EOL after else")) {
        destroyAST(cond);
        destroyAST(then);
        return NULL;
    }

    Node *elseBranch = parseBlock(parser);
    if (!elseBranch) {
        destroyAST(cond);
        destroyAST(then);
        return NULL;
    }

    Node *node = createAST(AST_IF, line);
    if (!node) {
        destroyAST(cond);
        destroyAST(then);
        destroyAST(elseBranch);
        handleError(&parser->error, INTERNAL_ERROR, "Failed to allocate if node", line);
        return NULL;
    }

    node->data.ifNode.cond = cond;
    node->data.ifNode.branchThen = then;
    node->data.ifNode.branchElse = elseBranch;

    return node;
}

TokenType statementFortune(Parser *parser) {
    if (!parser->fortuneTell) {
        parser->fortune = nextToken(&parser->error);
        parser->fortuneTell = true;
    }
    return parser->fortune.type; // vrati typ tokenu predpovedi:D
}

// STATEMENT
Node *parseStatement(Parser *parser) {
    TokenType type = parser->currentToken.type;
    Node *statement;

    switch (type) {
        case LPAREN:
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_INT_LITERAL:
        case TOKEN_DOUBLE_LITERAL:
        case TOKEN_STRING_LITERAL:
            statement = parseExpression(parser);
            break;

        case TOKEN_KW_VAL:
        case TOKEN_KW_VAR:
            statement = parseDecl(parser);
            break;

        case TOKEN_ID: {
            if (statementFortune(parser) == TOKEN_ASSIGN) {
                Node *id = parseId(parser);
                if (!id) return NULL;

                statement = parseAssignment(parser, id);
            } else {
                statement = parseExpression(parser);
            }
            break;
        }

        case TOKEN_KW_RETURN:
            statement = parseReturn(parser);
            break;

        case TOKEN_KW_IF:
            return parseIf(parser);
            break;

        case TOKEN_KW_WHILE:
            return parseWhile(parser);
            break;

        case TOKEN_LPAREN:
            statement = parseExpression(parser);
            break;

        default:
            handleError(&parser->error, SYN_ERROR, "Expected statement", parser->currentToken.lineNumber);
            break;
    }

    if (!statement) return NULL;

    if (!want(parser, TOKEN_EOL, "Expected EOL after statement")) {
        destroyAST(statement);
        return NULL;
    }

    return statement;
}
