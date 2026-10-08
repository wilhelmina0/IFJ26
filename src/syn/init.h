/**
* @file init.h
 * @brief Structure of parser
 *
 *
 *
 * @authors
 * - Bára Wilhelmová(xwilheb00)
 *
 */

#ifndef SYN_INIT_H
#define SYN_INIT_H

#include "../lex/init.h"   // temporary -> token has to be shared
#include "../ast/ast.h"    // AST structure
#include "../core/error.h" // error handling

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    Token currentToken;
    Token previousToken;

    Error error;
} Parser;

void parserInit(Parser *parser);  // sets the current token w/ nextToken() from lex
void parseNext(Parser *parser);   // moves onto the next token

bool checkType(Parser *parser, TokenType type);                // checks the token type
bool same(Parser *parser, TokenType type);                      // checks type moves onto next
bool want(Parser *parser, TokenType type, const char *message); // wants certain token –> error otherwise

bool parseProlog(Parser *parser); // checks prolog
Node *parseProgram(Parser *parser); // checks prolog, functions and EOF

bool parseFunction(Parser *parser); //(temporary bool before ast ops are finished;D)
Node *parseStatement(Parser *parser);
Node *parsePrint(Parser *parser);
Node *parseLiteral(Parser *parser);

#endif //SYN_INIT_H
