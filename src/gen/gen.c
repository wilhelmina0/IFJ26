/**
 * @file gen.c
 * @brief Structure of AST
 *
 * generates IFJcode26 and contains support data structures
 *
 * @authors
 * - Josef Kalousek - 279279
 *
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include "init.h"

//macro for arbitrary values so they don't appear so magical
//increase for less kernel calls
#define  DEFAULTSIZE 512

//betterString_t and its operations

typedef struct {
    char *data;
    uint32_t top;       //index of first NULL byte
    uint32_t capacity;  //amount of allocated data units
} betterString_t;

static inline betterString_t BSCtor(){
    betterString_t string;

    string.data = malloc(DEFAULTSIZE*sizeof(char));
    if(string.data == NULL) exit(99);

    string.capacity = DEFAULTSIZE;
    string.top = 0;

    for (uint32_t i = 0; i < DEFAULTSIZE; i++)
        string.data[i] = 0;

    return string;
}

//appends betterString_t with a C string
//allocates memory as needed
//TODO eliminate 2 unneccesary kernel calls
static inline void appendF(betterString_t *dst, char *src, ...){
    va_list args, copy;

    va_start(args, src);
    va_copy(copy, args);
    int32_t expLen = vsnprintf(NULL, 0, src, args);
    if(expLen < 0) exit(99);

    char *expStr = malloc((expLen + 1)*sizeof(char));
    if(expStr == NULL) exit(99);

    vsnprintf(expStr, expLen, src, copy);

    va_end(args);
    va_end(copy);

    while(dst->top + expLen + 1 >= dst->capacity){
        void *temp = realloc(dst->data, 2*dst->capacity*sizeof(char));
        if(temp == NULL) exit(99);

        dst->data = temp;
        dst->capacity = 2*dst->capacity;

        for(uint32_t i = dst->top + 1; i < dst->capacity; i++)
            dst->data[i] = 0;
    }

    for(uint32_t i = 0; i < expLen; i++)
        dst->data[dst->top++] = expStr[i];

    free(expStr);
}

static inline void BSDtor(betterString_t *string){

    free(string->data);
    string->capacity = 0;
    string->top = 0;
    string->data = NULL;

}

//stack and its operations

typedef struct{
    enum {
        INT,
        TEMP,
        REAL,
        STR,
        VAR
    }type;

    union {
        int64_t tempNum;
        double real;
        char *retString;
    } value;

} retVal_t;

//all non-static variabled gen needs
typedef struct {
    retVal_t left;
    betterString_t buffer;
    Node *currentNode;
    uint32_t childCTR;
    bool new;

} frame_t;

typedef struct {
    frame_t *data;
    uint32_t top;       //index of the active frame
    uint32_t capacity;  //contains max amount od frames
} stack_t;

//creates stack
static inline stack_t stackCtor(){
    stack_t stack;

    stack.data = malloc(DEFAULTSIZE*sizeof(frame_t));
    if(stack.data == NULL) exit(99);

    stack.top = 0;
    stack.capacity = DEFAULTSIZE;

    return stack;
}

//makes space for one frame above current top
//and returns pointer to it
static inline frame_t *prepFrame(stack_t *stack){
    if (stack->top+1 >= stack->capacity) {
        void *temp = realloc(stack->data, 2*stack->capacity*sizeof(frame_t));
        if(temp == NULL) exit(99);

        stack->data = temp;
        stack->capacity = 2*stack->capacity;
    }

    char *temp = (char *)&stack->data[stack->top+1];

    for(int i = 0; i < sizeof(frame_t); i++)
        temp[i] = 0;

    stack->data[stack->top + 1].new = true;

    return &stack->data[stack->top + 1];
}

//activates prepared frame
static inline void pushFrame(stack_t *stack){
    stack->top++;
}

//activates frame below curret top
//returns new active frame
static inline frame_t *popFrame(stack_t *stack){
    if(stack->top == 0) return NULL;

    frame_t *frame = &stack->data[--stack->top];
    return frame;
}

//returns pointer to active frame
static inline frame_t *readFrame(stack_t *stack){
    return &stack->data[stack->top];
}

static inline void stackDtor(stack_t *stack){

    free(stack->data);
    stack->capacity = 0;
    stack->top = 0;
    stack->data = NULL;

}

