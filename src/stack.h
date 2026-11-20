/**
* @file stack.h
 * @brief Stack abstract structure
 *
 * Stack structure is handed down from structure List expanded by pop and push operations.
 *
 * @authors
 * - Bára Wilhelmová (xwilheb00)
 *
 */

#ifndef IFJ26_STACK_H
#define IFJ26_STACK_H

#include <stddef.h>
#include "list.h"

typedef struct {
    List list;
} Stack;

static inline bool stackInit(Stack *stack, DT type) {
    return stack && listInit(&stack->list, type);
}

static inline bool stackIsEmpty(Stack *stack) {
    return listIsEmpty(&stack->list);
}

static inline size_t stackSize(Stack *stack) {
    return listSize(&stack->list);
}

static inline void *stackTop(Stack *stack) {
    ListNode *top = stack->list.first;
    if (!top) return NULL;

    return top->data;
}

static inline bool stackPush(Stack *stack, void *data) {
    return insertFirst(&stack->list, data);
}

static inline bool stackPop(Stack *stack, void *node) {
    List *list = &stack->list;
    ListNode *top = list->first;

    if (!node || !top) return false;

    memcpy(node, top->data, list->type.size);
    list->first = top->next;

    if (!list->first) list->last = NULL;
    if (list->active == top) list->active = NULL;

    list->size--;
    free(top->data);
    free(top);

    return true;
}

static inline void stackClean(Stack *stack) {
    listClean(&stack->list);
}

#endif // IFJ26_STACK_H
