/**
 * @file symtable.c
 * @brief AVL symbol table
 *
 *
 * @authors
 * - Radim Winter
 */

#include "symtable.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct TreeNode {
    char *name;
    Symbol value;
    struct TreeNode *left;
    struct TreeNode *right;
    int height;
} TreeNode;

struct SymTable {
    TreeNode *root;
};

static int height(const TreeNode *node) { return node ? node->height : 0; }

static void updateHeight(TreeNode *node) {
    int leftHeight = height(node->left);
    int rightHeight = height(node->right);
    node->height = 1 + (leftHeight > rightHeight ? leftHeight : rightHeight);
}

static int balance(const TreeNode *node) { return height(node->right) - height(node->left); }

static TreeNode *rotateLeft(TreeNode *root) {
    TreeNode *pivot = root->right;
    root->right = pivot->left;
    pivot->left = root;

    // update the old root height before the new root
    updateHeight(root);
    updateHeight(pivot);
    return pivot;
}

static TreeNode *rotateRight(TreeNode *root) {
    TreeNode *pivot = root->left;
    root->left = pivot->right;
    pivot->right = root;
    // update the old root height before the new root
    updateHeight(root);
    updateHeight(pivot);
    return pivot;
}

static TreeNode *rebalance(TreeNode *root) {
    updateHeight(root);
    int difference = balance(root);

    // left subtree too tall, choose a single or double rotation
    if (difference < -1) {
        if (balance(root->left) > 0) {
            root->left = rotateLeft(root->left); // first step of the double rotation
        }
        return rotateRight(root);
    }
    // right subtree too tall, choose a single or double rotation
    if (difference > 1) {
        if (balance(root->right) < 0) {
            root->right = rotateRight(root->right); // first step of the double rotation
        }
        return rotateLeft(root);
    }
    return root;
}

static TreeNode *createTreeNode(const char *name, Symbol value) {
    size_t length = strlen(name);
    if (length == SIZE_MAX) {
        return NULL;
    }
    TreeNode *node = malloc(sizeof(*node));
    if (!node) {
        return NULL;
    }
    // separate name copy owned by the tree node
    node->name = malloc(length + 1);
    if (!node->name) {
        free(node);
        return NULL;
    }
    memcpy(node->name, name, length + 1);
    node->value = value;
    node->left = NULL;
    node->right = NULL;
    node->height = 1;
    return node;
}

static TreeNode *insertNode(TreeNode *root, const char *name, Symbol value, ErrorCode *code) {
    if (!root) {
        TreeNode *node = createTreeNode(name, value);
        *code = node ? SUCCESS : INTERNAL_ERROR;
        return node;
    }
    int order = strcmp(name, root->name);
    if (order < 0) {
        root->left = insertNode(root->left, name, value, code);
    } else if (order > 0) {
        root->right = insertNode(root->right, name, value, code);
    } else {
        *code = SEM_ERROR_REDEFINED;
    }

    // rebalance on the way back from a successful insertion
    return *code == SUCCESS ? rebalance(root) : root;
}

static void destroyNodes(TreeNode *root) {
    if (!root) {
        return;
    }
    // free both subtrees before their parent node
    destroyNodes(root->left);
    destroyNodes(root->right);
    free(root->name);
    free(root);
}

SymTable *symtableCreate(void) {
    SymTable *table = malloc(sizeof(*table));
    if (table) {
        table->root = NULL;
    }
    return table;
}

void symtableDestroy(SymTable *table) {
    if (table) {
        destroyNodes(table->root);
        free(table);
    }
}

ErrorCode symtableInsert(SymTable *table, const char *name, Symbol value) {
    if (!table || !name) {
        return INTERNAL_ERROR;
    }
    ErrorCode code = SUCCESS;
    table->root = insertNode(table->root, name, value, &code);
    return code;
}

bool symtableFind(const SymTable *table, const char *name, Symbol *out) {
    if (!table || !name || !out) {
        return false;
    }
    const TreeNode *node = table->root;
    while (node) {
        int order = strcmp(name, node->name);
        if (order == 0) {
            *out = node->value;
            return true;
        }
        node = order < 0 ? node->left : node->right;
    }
    return false;
}
