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

/**
 * @brief Creates AST node of desired type.
 *
 * @param type The wanted AST node type.
 * @param line Line number
 *
 * @return created node
 */

static void destroyASTElement(void *element)
{
    Node *node = *(Node **)element;
    destroyAST(node);
}

static const DT astNodeType = {
    .size = sizeof(Node *),
    .destroy = destroyASTElement
};

Node *createAST(NodeType type, int line) {
    Node *node = malloc(sizeof(Node));

    if(!node) return NULL;

    node->type = type;
    node->line = line;

    node->children = NULL;
    node->childCount = 0;
    node->childCapacity = 0;

    memset(&node->data, 0, sizeof(node->data));

    switch (type) {
        case AST_PROGRAM:
            listInit(&node->data.program.declarations, astNodeType);
            break;
        case AST_FUNCTION:
            listInit(&node->data.function.params, astNodeType);
            node->data.function.body = NULL;
            break;
        case AST_BLOCK:
            listInit(&node->data.block.statements, astNodeType);
            break;
        case AST_CALL:
            listInit(&node->data.call.args, astNodeType);
            break;
    }

    return node;
}

/**
 * @brief Adds a child node to parent.
 *
 * @param parent pointer on parent node
 * @param child pointer on child node
 *
 * @return 0 on success, 1 on faiů
 */
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

/**
 * @brief Destroys AST node.
 *
 * @param node pointer to the node we want to destroy
 */
void destroyAST(Node *node) {
    if(!node) return;

    switch(node->type) {
        case AST_PROGRAM:
            listClean(&node->data.program.declarations);
            break;

        case AST_IDENTIFIER:
            free(node->data.identifier.id);
            break;

        case AST_STRING:
            free(node->data.stringLiteral.string);
            break;

        case AST_ASSIGN:
            free(node->data.assignment.name);
            destroyAST(node->data.assignment.value);
            break;

        case AST_VARDECL:
            free(node->data.varDeclaration.id);
            destroyAST(node->data.varDeclaration.initializer);
            break;

        case AST_FUNCTION:
            free(node->data.function.id);
            listClean(&node->data.function.params);
            destroyAST(node->data.function.body);
            break;

        case AST_PARAM:
            free(node->data.parameter.id);
            break;

        case AST_BLOCK:
            listClean(&node->data.block.statements);
            break;

        case AST_CALL:
            free(node->data.call.id);
            listClean(&node->data.call.args);
            break;

        case AST_BINARY:
            destroyAST(node->data.binary.left);
            destroyAST(node->data.binary.right);
            break;

        case AST_INT:
        case AST_DOUBLE:
        case AST_UNIT:
            break;

        case AST_RETURN:
            destroyAST(node->data.returnNode.value);
            break;

        case AST_IF:
            destroyAST(node->data.ifNode.cond);
            destroyAST(node->data.ifNode.branchThen);
            destroyAST(node->data.ifNode.branchElse);
            break;

        case AST_WHILE:
            destroyAST(node->data.whileNode.cond);
            destroyAST(node->data.whileNode.body);
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

/**
 * @brief Prints AST nodes.
 *
 * @param node
 * @param depth fo better visual representation
 */
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
        case AST_PARAM:
            printf("PARAMETER: %s", node->data.parameter.id);
            break;
        case AST_CALL:
            printf("CALL: %s", node->data.call.id);
            break;
        case AST_IDENTIFIER:
            printf("IDENTIFIER: %s", node->data.identifier.id);
            break;
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
            printf("ASSIGN: %s ", node->data.assignment.name);
            break;
        case AST_BINARY:
            printf("BINARY: %d", (int)node->data.binary.op);
            break;
        case AST_BLOCK:
            printf("BLOCK");
            break;
        case AST_VARDECL:
            printf("VARDECL: %s", node->data.varDeclaration.id);
            break;
        case AST_UNIT:
            printf("UNIT");
            break;
        case AST_IF:
            printf("IF");
            break;
        case AST_WHILE:
            printf("WHILE");
            break;
        case AST_RETURN:
            printf("RETURN");
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