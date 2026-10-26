/**
 * @file syn.c
 * @brief Syntactical analysis
 *
 * Parses token stream (using top to bottom recursive descent)
 * and then creates AST nodes.
 *
 * @authors
 * - Bára Wilhelmová(xwilheb00)
 *
 */


#include "init.h"

// precedence stack
void initStack(Stack *stack) {
    stack->el = NULL;
    stack->top = 0;
    stack->capacity = 0;
}

bool pushStack(Stack *stack, StackEl el) {
    if (stack->top >= stack->capacity) {
        size_t newCap = stack->capacity;

        if (newCap == 0) {
            newCap = 8;
        } else {
            newCap *= 2;
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

bool popStack(Stack *stack) {
    if (stack->top > 0) {
        stack->top--;
        return true;
    }
    return false;
}

StackEl *topStack(Stack *stack) {
    if (stack->top == 0) {
        // parser musi zavolat error!!!
        return NULL;
    }
    return &stack->el[stack->top - 1];
}

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

// lex dava syn vlastnictvi -> nutno vycistit tokeny po syntakticke analyze
void synClean(Parser *parser) {
    freeToken(&parser->previousToken);
    freeToken(&parser->currentToken);
}

void parserInit(Parser *parser) {
    parser->error.code =  SUCCESS;
    parser->error.message = "";
    parser->error.line = 0;

    // compound literal that uses designated initializer -> creates temporary object Token whose value we assign to prev
    parser->previousToken = (Token){.type = TOKEN_EOF}; // we need to initialize it so we can safely call freeToken
    parser->currentToken = nextToken();
}

void parseNext(Parser *parser){
    freeToken(&parser->previousToken);

    parser->previousToken = parser->currentToken;
    parser->currentToken = nextToken();
}

// parser utils
bool checkType(Parser *parser, TokenType type) {
    return parser->currentToken.type == type;
}

// move if u found desired token
bool same(Parser *parser, TokenType type) {
    if (!checkType(parser, type)) return false;

    parseNext(parser);
    return true;
}

// wanted token
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

// FUNCTION
bool parseFunction(Parser *parser){

    bool main = false;
    char *id = NULL;

    if(same(parser, TOKEN_ANNOTATION_MAIN)){
        main = true;
        id = "main";
    }

    if(!want(parser, TOKEN_KW_DEF, "Expected 'def'")){
        return false;
    }

    if(checkType(parser, TOKEN_ID)){
        id = parser->currentToken.stringValue;
        parseNext(parser);
    } else {
        // error
        return false;
    }

    if(!want(parser, TOKEN_LPAREN, "Expected '('")) return false;
    // sem prijdou params
    if(!want(parser, TOKEN_RPAREN, "Expected ')'")) return false;

    return true;
}

// PROGRAM
Node *parseProgram(Parser *parser) {
    if(!parseProlog(parser)) return NULL;
    if(!parseFunction(parser)) return NULL;
    if(!checkType(parser, TOKEN_EOF)) return NULL;

    Node *program = createAST(AST_PROGRAM, parser->currentToken.lineNumber);
    return program; // vraci root node ast
}

// LITERAL (INT, DOUBLE, STRING, UNIT?)
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

    id->data.identifier.id= copyStr(parser->currentToken.stringValue);
    if (!id->data.identifier.id) {
        handleError(&parser->error, INTERNAL_ERROR, "Failed to make a copy of identifier", parser->currentToken.lineNumber);
        destroyAST(id);
        return NULL;
    }

    parseNext(parser);
    return id;
}

Node *parseCall(Parser *parser, Node *id) {

    int line = id->line;
    Node *call = createAST(AST_CALL, line);

    if (!call) {
        handleError(&parser->error, INTERNAL_ERROR, "Failed to allocate 'call' node", parser->currentToken.lineNumber);
        return NULL;
    }
    // str ownership transfer
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

            if (!addChild(call, arg)) {
                handleError(&parser->error, INTERNAL_ERROR, "Failed to add child node", line);
                destroyAST(arg);
                destroyAST(call);
                return NULL;
            }
        } while (same(parser, TOKEN_COMMA));

    }

    if (!want(parser, TOKEN_RPAREN, "Expected ')'")) {
        destroyAST(call);
        return NULL;
    }

    return call;
}

static const char precedenceTable[6][6] = {
    // > ... reduce - neposouvej hranici redukuj vstup nespotrebuj token
    // < ... shift - vloz hranici a vstup
    // ! ... invalid operation combination
    // = ... insert input and shift
    // E ... end state

    // operand add_sub mul_div lparen rparen end
    // operand
    {'!', '>', '>', '!', '>', '>'},
    // add_sub
    {'<', '>', '<', '<', '>', '>'},
    // mul_div
    {'<', '>', '>', '<', '>', '>'},
    // lparen
    {'<', '<', '<', '<', '=', '!'},
    // rparen
    {'!', '>', '>', '!', '>', '>'},
    // ex_end
    {'<', '<', '<', '<', '!', 'E'},
};

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
        item.node = parseLiteral(parser);
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

    // expr operator expr
    if (count == 3) {
        StackEl *left = &stack->el[borderIdx + 1];
        StackEl *mid = &stack->el[borderIdx + 2];
        StackEl *right = &stack->el[borderIdx + 3];

        if (left->symbol == EXPR && right->symbol == EXPR && (mid->symbol == ADD_SUB || mid->symbol == MUL_DIV)) {
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
        if (left->symbol == LPAREN && right->symbol == RPAREN && mid->symbol ==  EXPR) {
            StackEl result = *mid;

            stack->el[borderIdx] = result;
            stack->top = borderIdx + 1;
            return true;
        }

    }

    handleError(&parser->error, SYN_ERROR, "Invalid expression reduction", parser->currentToken.lineNumber);
    return false;
}

Node *parseExpression(Parser *parser) {
    Token *token = &parser->currentToken;
    int line = token->lineNumber;
    Stack stack;
    OpSymbol symbol;
    StackEl bottom = {.symbol = END, .line = 0};


    initStack(&stack);
    if (!pushStack(&stack, bottom)) {
        handleError(&parser->error, INTERNAL_ERROR, "Failed to push stack", line);
        return NULL;
    }

    while (true) {
        switch (token->type) {
            case TOKEN_PLUS:
            case TOKEN_MINUS:
                symbol = ADD_SUB;
                break;
            case TOKEN_MUL:
            case TOKEN_DIV:
                symbol = MUL_DIV;
                break;
            case TOKEN_INT_LITERAL:
            case TOKEN_DOUBLE_LITERAL:
            case TOKEN_STRING_LITERAL:
                symbol = OPERAND;
                break;
            case TOKEN_LPAREN:
                symbol = LPAREN;
                break;
            case TOKEN_RPAREN:
                symbol = RPAREN;
                break;
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
        // tady byl stary blok

    }

    //Node *expression = createAST(AST_EXPRESSION, token->lineNumber);

}

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

    Node *value = parseLiteral(parser); // potom parse expression
    if (!value) {
        destroyAST(assign);
        return NULL;
    }

    assign->data.assignment.value = value;
    return assign;
}

Node *parseTerm(Parser *parser) {
    Token *token = &parser->currentToken;
    //TokenType type;

    switch (token->type) {
        case TOKEN_INT_LITERAL:
        case TOKEN_DOUBLE_LITERAL:
        case TOKEN_STRING_LITERAL:
            parseLiteral(parser);
            break;
        case TOKEN_ID: {
            Node *id = parseId(parser);
            if (!id) {
                handleError(&parser->error, INTERNAL_ERROR, "Failed to allocate identifier node", parser->currentToken.lineNumber);
                return NULL;
            }

            // if next one is ( -> function call
            if (checkType(parser, TOKEN_LPAREN)) {
                return parseCall(parser, id);
            } else if (checkType(parser, TOKEN_ASSIGN)) {
                return parseAssignment(parser, id);
            }

            return id;
        }

        default:
            handleError(&parser->error, SYN_ERROR, "Expected expression", parser->currentToken.lineNumber);
            break;
    }

}

// STATEMENT
Node *parseStatement(Parser *parser) {
    if (checkType(parser, TOKEN_ID))
        return parseCall(parser, parseId(parser));

    handleError(&parser->error, SYN_ERROR, "Expected statement", parser->currentToken.lineNumber);

    return NULL;
}
