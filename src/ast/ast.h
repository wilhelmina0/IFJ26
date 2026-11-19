/**
 * @file ast.h
 * @brief Structure of AST
 *
 * Defines the structure and lifecycle of AST.
 *
 * @authors
 * - Bára Wilhelmová(xwilheb00)
 *
 */

#ifndef AST_H
#define AST_H

#include "../lex/init.h"
#include "../core/list.h"

#include "stdbool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// AST node types
typedef enum {
    AST_PROGRAM,
    AST_FUNCTION,
    AST_PARAM,
    AST_CALL,
    AST_BLOCK,

    AST_ASSIGN,
    AST_VARDECL,

    AST_IDENTIFIER,

    AST_INT,
    AST_DOUBLE,
    AST_STRING,
    AST_UNIT,

    AST_BINARY,
    AST_UNARY,

    AST_IF,
    AST_WHILE,
    AST_RETURN
} NodeType;

typedef enum {
    TYPE_UNSPECIFIED = 0,
    TYPE_INT,
    TYPE_DOUBLE,
    TYPE_STRING,
    TYPE_UNIT
} DeclType;

struct Symbol;

// structure of AST node
typedef struct Node Node;
struct Node{

     NodeType type;
     int line;

     Node **children; // for nodes with variable child count
     size_t childCount;
     size_t childCapacity;

     union {
         // PROGRAM
         struct {
            List declarations;
         } program;

         // LITERALS
         struct {
             char *id;
         } identifier;

         struct {
             int value;
         } intLiteral;

         struct {
             double value;
         } doubleLiteral;

         struct {
             char *string;
         } stringLiteral;

         // FUNCTION, PARAM, BLOCK, CALL
         struct {
             char *id;
             bool main;
             List params;
             Node *body;
             DeclType returnType;
             struct Symbol *symbol;
         } function;

         struct {
             char *id;
             DeclType declType;
             struct Symbol *symbol;
         } parameter;

         struct {
             List statements;
         } block;

         struct {
             char *id;
             List args;
             struct Symbol *symbol;
         } call;

         // ASSIGNMENT
         struct {
             char *name;
             Node *value;
         } assignment;

         // VARIABLE DECLARATION
         struct {
             char *id;
             Node *initializer;
             bool mutable;
             DeclType declType;
             struct Symbol *symbol;
         } varDeclaration;

         // EXPRESSION
         struct {
             TokenType op;
             Node *left, *right;
         } binary;

         struct {
             TokenType op;
             Node *operand;
         } unary;

         // RETURN
         struct {
             Node *value; // can be NULL
         } returnNode;

         // IF, WHILE
         struct {
             Node *cond;
             Node *branchThen;
             Node *branchElse;
         } ifNode;

         struct {
             Node *cond;
             Node *body;
         } whileNode;

     } data;
};


// ast constructor
Node *createAST(NodeType type, int line);
int addChild(Node *parent, Node *child);

// destructor
void destroyAST(Node *ast);

void printAST(Node *node, size_t depth);
#endif //AST_H
