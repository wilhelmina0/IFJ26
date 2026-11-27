/**
* @file syn.h
 *
 * @brief Structure of parser
 *
 * @authors
 * - Bára Wilhelmová (xwilheb00)
 *
 */

#ifndef SYN_INIT_H
#define SYN_INIT_H

#include "token.h"
#include "ast.h"
#include "error.h"


#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ======================== PARSER ========================== */
typedef struct {
    Token currentToken;
    Token previousToken;

    bool fortuneTell; // rika jestli je treba znat hodnotu dalsiho tokenu
    Token fortune; // "predpovi" dalsi token bez toho aniz by ho snedl

    Error error;
} Parser;

void freeToken(Token *token);   // for tokens that use strdup()
void synClean(Parser *parser);  // when parser is finished

void parserInit(Parser *parser);  // sets the current token w/ nextToken() from lex
bool parseNext(Parser *parser);   // moves onto the next token

bool checkType(Parser *parser, TokenType type);                // checks the token type
bool same(Parser *parser, TokenType type);                      // checks type moves onto next
bool want(Parser *parser, TokenType type, const char *message); // wants certain token –> error otherwise

// copies string values for ast
char *copyStr(const char *str);

bool parseProlog(Parser *parser);       // checks prolog
Node *parseProgram(Parser *parser);     // checks prolog, functions and EOF

Node *parseFunction(Parser *parser);  // function
Node *parseBlock(Parser *parser);     // block
TokenType statementFortune(Parser *parser);
Node *parseStatement(Parser *parser);

Node *parseCall(Parser *parser, Node *id);    // for function calls (print, ...)
Node *parseExpression(Parser *parser); // expr
Node *parseAssignment(Parser *parser, Node *id); // assign

Node *parseReturn(Parser *parser); // return
Node *parseDecl(Parser *parser);  // var / val declaration

bool parseParams(Parser *parser, List *params); // parameters
bool parseType(Parser *parser, DeclType *type); // data type

Node *parseLiteral(Parser *parser); // literals
Node *parseTerm(Parser *parser);    // terminals
Node *parseId(Parser *parser);      // identifiers

Node *parseIf(Parser *parser);      // if then else
Node *parseWhile(Parser *parser);   // while loop

/* ======================== PRECEDENCE ========================== */
// for expressions
typedef enum {
    UNARY,
    OPERAND,
    ADD_SUB, // + -
    MUL_DIV, // * /
    LPAREN,
    RPAREN,
    COMP,   // < <= > >=
    // && a ||
    EQUAL,  // == !=
    END,    // aka $ (end of expression)
    EXPR,   // expr
    BORDER, // border
} OpSymbol;

// stack
typedef struct {
    OpSymbol symbol;
    TokenType op;
    Node *node;
    int line;
} StackEl;

typedef struct {
    StackEl *el;
    size_t top; // num of elements, top = el[top-1]
    size_t capacity;
    Error error;
} Stack;

// stack operations
void initStack(Stack *stack);
bool pushStack(Stack *stack, StackEl el);
bool popStack(Stack *stack);
StackEl *topStack(Stack *stack);

bool shiftStack(Parser *parser, Stack *stack, char action, OpSymbol symbol, size_t termIdx);
bool reduceStack(Parser *parser, Stack *stack);
bool insertBorder(Stack *stack,  size_t termIdx, int line);

#endif //SYN_INIT_H
