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

//upozornění pro developera tady toho skvělýho parseru: mallocovaný data z tokenů musíš freeovat ty :DDD

#include "init.h"


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

    /* old
    parser->error.message = message;
    parser->error.code = SYN_ERROR; */

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

Node *parseCall(Parser *parser) {
    if (!checkType(parser, TOKEN_ID)) {
        handleError(&parser->error, SYN_ERROR, "Expected identifier", parser->currentToken.lineNumber);
        return NULL;
    }
    int line = parser->currentToken.lineNumber;
    Node *call = createAST(AST_CALL, line);

    if (!call) {
        handleError(&parser->error, INTERNAL_ERROR, "Failed to allocate 'call' node", parser->currentToken.lineNumber);
        return NULL;
    }

    call->data.call.id = copyStr(parser->currentToken.stringValue); // ast string copy of id before want -> moves onto the next token
    if (!call->data.call.id) {
        // error id is null
        handleError(&parser->error, SYN_ERROR, "Identifier is NULL", line);
        destroyAST(call);
        return NULL;
    }
    parseNext(parser);

    if (!want(parser, TOKEN_LPAREN, "Expected '('")) {
        destroyAST(call);
        return NULL;
    }

    // if u do not find ) -> expect params
    if (!checkType(parser, TOKEN_RPAREN)) {
        Node *param = parseLiteral(parser); // temporary until i finish expressions, currently accepts only 1 param!!
        if (!param) {
            destroyAST(call);
            return NULL;
        }

        if (!addChild(call, param)) {
            handleError(&parser->error, INTERNAL_ERROR, "Failed to add child node", line);
            destroyAST(param);
            destroyAST(call);
            return NULL;
        }
    }

    if (!want(parser, TOKEN_RPAREN, "Expected ')'")) {
        destroyAST(call);
        return NULL;
    }

    return call;
}


// STATEMENT
Node *parseStatement(Parser *parser) {
    if (checkType(parser, TOKEN_ID)) return parseCall(parser);

    handleError(&parser->error, SYN_ERROR, "Expected statement", parser->currentToken.lineNumber);

    return NULL;
}
