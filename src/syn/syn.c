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

void parserInit(Parser *parser) {
    parser->error.code =  SUCCESS;
    parser->error.message = "";

    parser->currentToken = nextToken();
}

void parseNext(Parser *parser){
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

    // error handling
    parser->error.message = message;
    parser->error.code = SYN_ERROR;

    return false;
}

// MINI AST
// PROLOG - je povinny takze staci bool neni treba pro nej vytvaret node
bool parseProlog(Parser *parser) {

    if(!same(parser, TOKEN_KW_IMPORT)){
        return false;
    }

    if(!same(parser, TOKEN_ID)){
        return false;
    }

    if(!same(parser, TOKEN_DOT)){
        return false;
    }

	if(!same(parser, TOKEN_MUL)){
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
    } else if(checkType(parser, TOKEN_ID)){
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

    //Node *program = createProgram(); + tam naparsuj funkce v listu
    //return program; // vraci root node ast
}

// LITERAL (INT, DOUBLE, STRING, UNIT?)
Node *parseLiteral(Parser *parser) {
    Token token = parser->currentToken;

    switch (token.type) {
        case TOKEN_TYPE_INT:
        case TOKEN_TYPE_DOUBLE:
        case TOKEN_TYPE_STRING:
            parseNext(parser);
            /*
                Node *lit =  createLiteral(token); z ast
                parseNext(parser);
                return lit;
            */

            return NULL;

        default:
            return NULL;
    }

}

// PRINT
Node *parsePrint (Parser *parser) {
    if (!checkType(parser, TOKEN_ID) || strcmp(parser->currentToken.stringValue, "print")) NULL;
    parseNext(parser);
    if(!want(parser, TOKEN_LPAREN, "Expected '('")) return NULL;

    Node *lit = parseLiteral(parser);
    if(!lit) return NULL;

    if(!want(parser, TOKEN_RPAREN, "Expected ')'")) return NULL;
    // return createPrint(lit); potom
    return lit;
}

// STATEMENT
Node *parseStatement(Parser *parser) {
    if (checkType(parser, TOKEN_ID) || !strcmp(parser->currentToken.stringValue, "print")) return parsePrint(parser); // zatim pro print
    return NULL;
}
