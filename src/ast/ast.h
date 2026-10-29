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

#include "stdbool.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// AST node types
typedef enum {
    AST_PROGRAM,
    AST_FUNCTION,
    AST_CALL,
    AST_BLOCK,

    AST_ASSIGN,

    AST_IDENTIFIER,

    AST_INT,
    AST_DOUBLE,
    AST_STRING,

    AST_BINARY
} NodeType;

// structure of AST node
typedef struct Node Node;
struct Node{

     NodeType type;
     int line;

     Node **children; // for nodes with variable child count
     size_t childCount;
     size_t childCapacity;

     union {

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

         struct {
             char *id;
             bool main;
         } function;

         struct {
             char *id;
         } call;

         struct {
             char *name;
             Node *value;
         } assignment;

         struct {
             TokenType op;
             Node *left, *right;
         } binary;

     } data;
};


// ast constructor
Node *createAST(NodeType type, int line);
int addChild(Node *parent, Node *child);

// destructor
void destroyAST(Node *ast);

void printAST(Node *node, size_t depth);
#endif //AST_H
