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
            break;

        default:
            break;
    }

    // postupny destroy vsech children
    for(size_t i = 0; i < node->childCount; i++) {
        destroyAST(node->children[i]);
    }

    free(node->children);
    free(node);
}
