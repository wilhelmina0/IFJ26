/**
 * @file symtable.c
 * @brief AVL symbol table
 *
 * Inserts and finds symbols in a height-balanced binary search tree.
 * Updates heights and restores balance while returning from recursive insertion.
 * Based on IAL lecture 05_2026_Vyhledavani_II.pdf, slides 37-52.
 *
 * @authors
 * - Radim Winter
 *
 * @par AI prompt (English summary)
 * Adapt my semantics and symbol table to the supplied project materials and
 * colleague feedback. Use a balanced tree, use the actual AST interface,
 * and keep simple camelCase code with English documentation.
 * The original user wording is recorded in AI_USAGE.md.
 *
 * @note AI-generated reference code (ChatGPT); see AI_USAGE.md for the prompt.
 * The assignment prohibits AI-generated search structures in the submission.
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

/** @brief Returns zero for an empty subtree, otherwise its stored height. */
static int height(const TreeNode *node) { return node ? node->height : 0; }

/** @brief Recomputes a node's height from the heights of its children. */
static void updateHeight(TreeNode *node) {
    int leftHeight = height(node->left);
    int rightHeight = height(node->right);
    node->height = 1 + (leftHeight > rightHeight ? leftHeight : rightHeight);
}

/** @brief Returns right height minus left height, following the IAL slides. */
static int balance(const TreeNode *node) { return height(node->right) - height(node->left); }

/** @brief Rotates left and returns the new root of this subtree. */
static TreeNode *rotateLeft(TreeNode *root) {
    TreeNode *pivot = root->right;
    root->right = pivot->left;
    pivot->left = root;
    // The lower node must be updated before the new root.
    updateHeight(root);
    updateHeight(pivot);
    return pivot;
}

/** @brief Rotates right and returns the new root of this subtree. */
static TreeNode *rotateRight(TreeNode *root) {
    TreeNode *pivot = root->left;
    root->left = pivot->right;
    pivot->right = root;
    updateHeight(root);
    updateHeight(pivot);
    return pivot;
}

/** @brief Restores AVL balance after insertion, using a single or double rotation. */
static TreeNode *rebalance(TreeNode *root) {
    updateHeight(root);
    int difference = balance(root);

    if (difference < -1) {
        if (balance(root->left) > 0) {
            root->left = rotateLeft(root->left); // LR: first turn it into LL.
        }
        return rotateRight(root);
    }
    if (difference > 1) {
        if (balance(root->right) < 0) {
            root->right = rotateRight(root->right); // RL: first turn it into RR.
        }
        return rotateLeft(root);
    }
    return root;
}

/** @brief Allocates a leaf with its own copy of the name. */
static TreeNode *createTreeNode(const char *name, Symbol value) {
    size_t length = strlen(name);
    if (length == SIZE_MAX) {
        return NULL;
    }
    TreeNode *node = malloc(sizeof(*node));
    if (!node) {
        return NULL;
    }
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

/** @brief Inserts recursively, then rebalances each ancestor on the way back. */
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
    // Failed insertion has not changed the subtree, so its heights still hold.
    return *code == SUCCESS ? rebalance(root) : root;
}

/** @brief Frees children before their parent, leaving borrowed AST nodes alive. */
static void destroyNodes(TreeNode *root) {
    if (!root) {
        return;
    }
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
