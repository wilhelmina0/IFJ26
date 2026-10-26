/**
* @file ast.c
 * @brief Structure of AST
 *
 * Defines the structure and lifecycle of AST.
 *
 * @authors
 * - Bára Wilhelmová(xwilheb00)
 *
 */

#include "ast.h"

Node *createAST(NodeType type, int line) {
    Node *node = malloc(sizeof(Node));

    if(!node) return NULL;

    node->type = type;
    node->line = line;

    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;

    memset(&node->data, 0, sizeof(node->data));

    return node;
}

int addChild(Node *parent, Node *child) {
    if(!parent || !child) return 0;

    if(parent->childCount >= parent->childCapacity) {
        size_t newCapacity = !parent->childCapacity ? 4 : parent->childCapacity * 2;

        Node **newChild = realloc(parent->children, newCapacity * sizeof(Node*));
        if (!newChild) return 0;

        parent->children = newChild;
        parent->childCapacity = newCapacity;
    }
    parent->children[parent->childCount++] = child;
    return 1;
}

void destroyAST(Node *node) {
    if(!node) return;

    switch(node->type) {
        case AST_IDENTIFIER:
            free(node->data.identifier.id);
            break;

        case AST_STRING:
            free(node->data.stringLiteral.string);
            break;

        case AST_ASSIGN:
            free(node->data.assignment.name);
            free(node->data.assignment.value);
            break;

        case AST_FUNCTION:
            free(node->data.function.id);
            break;

        case AST_CALL:
            free(node->data.call.id);
            break;

        default:
            break;
    }

    // recursive destroy of all children
    for(size_t i = 0; i < node->childCount; i++) {
        destroyAST(node->children[i]);
    }

    free(node->children);
    free(node);
}

void printAST(Node *node, size_t depth) {
    if(!node) return;
    for (size_t i = 0; i < depth; i++) printf("  ");

    switch(node->type) {
        case AST_PROGRAM:
            printf("PROGRAM");
            break;
        case AST_FUNCTION:
            printf("FUNCTION: %s", node->data.function.id);
            break;
        case AST_CALL:
            printf("CALL: %s", node->data.call.id);
            break;
        case AST_IDENTIFIER:
            printf("IDENTIFIER: %s", node->data.identifier.id);
        case AST_STRING:
            printf("STRING: \"%s\"", node->data.stringLiteral.string);
            break;
        case AST_DOUBLE:
            printf("DOUBLE: %g", node->data.doubleLiteral.value);
            break;
        case AST_INT:
            printf("INT: %d", node->data.intLiteral.value);
            break;
        case AST_ASSIGN:
            printf("ASSIGN");
            break;
        case AST_BLOCK:
            printf("BLOCK");
            break;

        default:
            printf("UNKNOWN");
            break;
    }
    printf(" (line: %d)\n", (int)node->line);
    // recursive node print
    for (size_t i = 0; i < node->childCount; i++) {
        printAST(node->children[i], depth + 1);
    }
}