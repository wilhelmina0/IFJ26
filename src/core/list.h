/**
* @file list.h
 * @brief List abstract structure
 *
 * Defines the structure and basic operations of single linked List.
 *
 * @authors
 * - Bára Wilhelmová (xwilheb00)
 *
 */

#ifndef IFJ26_LIST_H
#define IFJ26_LIST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    size_t size;
    void (*destroy)(void *);
} DT;

typedef struct ListNode {
    void *data;

    struct ListNode *next;
} ListNode;

typedef struct {
    ListNode *first;
    ListNode *last;
    ListNode *active;

    size_t size;
    DT type;
} List;

static inline bool listInit(List *list, DT type) {
    if (!list || !type.size) return false;

    *list = (List){0};
    list->type = type;
    return true;
}

static inline bool listIsEmpty(List *list) {
    return !list || list->size == 0;
}

static inline size_t listSize(List *list) {
    return (list->size);
}

static inline void listFirst(List *list) {
    list->active = list->first;
}

static inline void listLast(List *list) {
    list->active = list->last;
}

static inline void listNext(List *list) {
    if (list->active) list->active = list->active->next;
}

static inline bool isActive(List *list) {
    return (list->active != NULL);
}

static inline ListNode *createNode(List *list, void *data) {
    if (!list || !data || !list->type.size) return NULL;

    ListNode *node = malloc(sizeof(*node));
    if (!node) return NULL;

    node->data = malloc(list->type.size);
    if (!node->data) {
        free(node);
        return NULL;
    }

    memcpy(node->data, data, list->type.size);
    node->next = NULL;
    return node;
}

static inline void listDestroyNode(List *list, ListNode *node) {
    if (list->type.destroy) list->type.destroy(node->data);
    free(node->data);
    free(node);
}

static inline bool insertFirst(List *list, void *data) {
    ListNode *new = createNode(list, data);
    if (!new) return false;

    new->next = list->first;
    list->first = new;

    if (!list->last) list->last = new;

    list->size++;
    return true;
}

static inline bool insertAfter(List *list, void *data) {
    if (!isActive(list)) return false;

    ListNode *insert = createNode(list, data);
    if (!insert) return false;

    insert->next = list->active->next;
   list->active->next = insert;

    if (!insert->next) list->last = insert;

    list->size++;
    return true;
}

static inline bool deleteAfter(List *list) {
    if (!list->active || !list->active->next) return false;

    ListNode *delete = list->active->next;
    list->active->next = delete->next;
    if (!delete->next) list->last = list->active;

    list->size--;
    listDestroyNode(list, delete);

    return true;
}

static inline void listClean(List *list) {
    ListNode *node = list->first;

    while (node) {
        ListNode *next = node->next;
        listDestroyNode(list, node);
        node = next;
    }

    *list = (List){ .type = list->type }; // vynuluju cleny na adrese list
}

#endif // IFJ26_LIST_H
