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
#include <stdio.h>
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

    vsnprintf(expStr, expLen + 1, src, copy);

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
    List *extra;
    uint32_t childCTR;
    uint32_t reserve;
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



//actual code generation;
#define VARIABLECHILD_RECURSE                                                   \
    nextFrame = prepFrame(&stack);                                              \
    frame = readFrame(&stack);                                                  \
    nextFrame->currentNode = frame->currentNode->children[frame->childCTR++];   \
    pushFrame(&stack);                                                          \
    frame = nextFrame

#define STATIC_RECURSE(child)                           \
    nextFrame = prepFrame(&stack);                      \
    frame = readFrame(&stack);                          \
    nextFrame->currentNode = frame->currentNode->child; \
    pushFrame(&stack);                                  \
    frame = nextFrame

#define RECURSE_LIST(type)                              \
    nextFrame = prepFrame(&stack);                      \
    frame = readFrame(&stack);                          \
    nextFrame->currentNode = *(Node **)frame->currentNode->data.type.active->data;    \
    frame->childCTR++;                                  \
    pushFrame(&stack);                                  \
    frame = nextFrame

#define PREPLIST(type)                                  \
    listFirst(&frame->currentNode->data.type)

void printSymbol (retVal_t retVal, betterString_t *string){
    switch (retVal.type) {
        case VAR:
            appendF(string, "LF@_%s", retVal.value.retString);
            break;

        case INT:
            appendF(string, "int@%ld", retVal.value.tempNum);
            break;

        case TEMP:
            appendF(string, "LF@T%ld", retVal.value.tempNum);
            break;

        case REAL:
            appendF(string, "float@%a", retVal.value.real);
            break;

        case STR:
            appendF(string, "string@%s", retVal.value.retString);
            break;
    }
}

static const char *builtinCode =
//print(value: Any)
"label $$print\n"
"pushframe\n"
"defvar LF@v\n"
"pops LF@v\n"
"write LF@v\n"
"pushs nil@nil\n"
"popframe\n"
"return\n"

//readLine()
"label $$readLine\n"
"pushframe\n"
"defvar LF@r\n"
"read LF@r string\n"
"jumpifneq $$readLine_ok LF@r nil@nil\n"
"move LF@r string@\n"
"label $$readLine_ok\n"
"pushs LF@r\n"
"popframe\n"
"return\n"

//readInt()
"label $$readInt\n"
"pushframe\n"
"defvar LF@r\n"
"read LF@r int\n"
"jumpifneq $$readInt_ok LF@r nil@nil\n"
"move LF@r int@0\n"
"label $$readInt_ok\n"
"pushs LF@r\n"
"popframe\n"
"return\n"

//readDouble()
"label $$readDouble\n"
"pushframe\n"
"defvar LF@r\n"
"read LF@r float\n"
"jumpifneq $$readDouble_ok LF@r nil@nil\n"
"move LF@r float@0x0p+0\n"
"label $$readDouble_ok\n"
"pushs LF@r\n"
"popframe\n"
"return\n"

//toStr(value: Any)
"label $$toStr\n"
"pushframe\n"
"defvar LF@v\n"
"pops LF@v\n"
"defvar LF@t\n"
"type LF@t LF@v\n"
"jumpifeq $$toStr_int LF@t string@int\n"
"jumpifeq $$toStr_float LF@t string@float\n"
"jumpifeq $$toStr_nil LF@t string@nil\n"
"pushs LF@v\n"
"jump $$toStr_end\n"
"label $$toStr_int\n"
"int2str LF@v LF@v\n"
"pushs LF@v\n"
"jump $$toStr_end\n"
"label $$toStr_float\n"
"float2str LF@v LF@v\n"
"pushs LF@v\n"
"jump $$toStr_end\n"
"label $$toStr_nil\n"
"pushs string@()\n"
"label $$toStr_end\n"
"popframe\n"
"return\n"

//toInt(value: Double)
"label $$toInt\n"
"pushframe\n"
"defvar LF@v\n"
"pops LF@v\n"
"float2int LF@v LF@v\n"
"pushs LF@v\n"
"popframe\n"
"return\n"

//strLength(value: String)
"label $$strLength\n"
"pushframe\n"
"defvar LF@v\n"
"pops LF@v\n"
"strlen LF@v LF@v\n"
"pushs LF@v\n"
"popframe\n"
"return\n"

//subStr(value: String, start: Int, end: Int)
"label $$subStr\n"
"pushframe\n"
"defvar LF@e\n"
"pops LF@e\n"
"defvar LF@b\n"
"pops LF@b\n"
"defvar LF@s\n"
"pops LF@s\n"
"defvar LF@len\n"
"strlen LF@len LF@s\n"
"defvar LF@c\n"
"lt LF@c LF@b int@0\n"
"jumpifeq $$subStr_err LF@c bool@true\n"
"lt LF@c LF@e int@0\n"
"jumpifeq $$subStr_err LF@c bool@true\n"
"gt LF@c LF@b LF@e\n"
"jumpifeq $$subStr_err LF@c bool@true\n"
"lt LF@c LF@b LF@len\n"
"jumpifeq $$subStr_err LF@c bool@false\n"
"gt LF@c LF@e LF@len\n"
"jumpifeq $$subStr_err LF@c bool@true\n"
"defvar LF@r\n"
"move LF@r string@\n"
"defvar LF@ch\n"
"label $$subStr_loop\n"
"jumpifeq $$subStr_end LF@b LF@e\n"
"getchar LF@ch LF@s LF@b\n"
"concat LF@r LF@r LF@ch\n"
"add LF@b LF@b int@1\n"
"jump $$subStr_loop\n"
"label $$subStr_end\n"
"pushs LF@r\n"
"popframe\n"
"return\n"
"label $$subStr_err\n"
"exit int@40\n"

//ord(value: String, index: Int)
"label $$ord\n"
"pushframe\n"
"defvar LF@i\n"
"pops LF@i\n"
"defvar LF@s\n"
"pops LF@s\n"
"defvar LF@len\n"
"strlen LF@len LF@s\n"
"defvar LF@c\n"
"lt LF@c LF@i int@0\n"
"jumpifeq $$ord_bad LF@c bool@true\n"
"lt LF@c LF@i LF@len\n"
"jumpifeq $$ord_bad LF@c bool@false\n"
"stri2int LF@c LF@s LF@i\n"
"pushs LF@c\n"
"popframe\n"
"return\n"
"label $$ord_bad\n"
"pushs int@-1\n"
"popframe\n"
"return\n"

//chr(code: Int)
"label $$chr\n"
"pushframe\n"
"defvar LF@v\n"
"pops LF@v\n"
"int2char LF@v LF@v\n"
"pushs LF@v\n"
"popframe\n"
"return\n";

int findBuiltin(char *id){

    static const char *builtinNames[] = {
        "print",
        "readLine",
        "readInt",
        "readDouble",
        "toStr",
        "toInt",
        "strLength",
        "subStr",
        "ord",
        "chr",
    };

    for(int i = 0; i < 10; i++){
        if(!strcmp(builtinNames[i], id))return i+1;
    }

    return 0;

}

char *codeGen(Node top){
    static uint64_t LCTR = 0;
    static uint64_t tempCTR = 0;
    static bool call;

    retVal_t retVal;


    DeclType mode;
    stack_t stack = stackCtor();
    frame_t *frame = prepFrame(&stack);
    pushFrame(&stack);
    frame_t *nextFrame;
    betterString_t output = BSCtor();



    frame->currentNode = &top;
    //frame->new = true;

    while(true){
        switch (frame->currentNode->type) {
            //generates header
            //recuses on child nodes
            case AST_PROGRAM:
                if(frame->new == true){
                    frame->new = false;
                    appendF(&output, ".IFJcode26\n");


                    //finds @main function and calls it
                    PREPLIST(program.declarations);
                    while(frame->currentNode->data.program.declarations.active != NULL){
                        if((*(Node **)frame->currentNode->data.program.declarations.active->data)->data.function.main){
                            appendF(&output, "createframe\ncall $_%s\nexit int@0\n",
                                    (*(Node **)frame->currentNode->data.program.declarations.active->data)->data.function.id);
                            appendF(&output, "%s", builtinCode);
                            break;
                        }
                        listNext(&frame->currentNode->data.program.declarations);
                    }
                    PREPLIST(program.declarations);
                } else {
                listNext(&frame->currentNode->data.program.declarations);
                }

                if(frame->currentNode->data.program.declarations.active == NULL)
                    goto done;

                RECURSE_LIST(program.declarations);
                break;

            //creates label and return statement of a function
            //recuses to create function code
            //TODO figure out arguments
            case AST_FUNCTION:
                if (frame->new == true) {
                    appendF(&output, "label $_%s\npushframe\n", frame->currentNode->data.function.id);
                    frame->new = false;

                    //preparing reverse list
                    PREPLIST(function.params);
                    frame->extra = malloc(sizeof(List));
                    if(frame->extra == NULL){
                        handleError(NULL, 99, "failed allocation\n", 0);
                    }
                    listInit(frame->extra, (DT){sizeof(Node *), NULL});

                    //filling out reverse list
                    while (frame->currentNode->data.function.params.active->data != NULL) {
                        if(!insertFirst(frame->extra,
                            frame->currentNode->data.function.params.active->data))
                            handleError(NULL, 99, "failed making reverse list\n", 0);

                        listNext(&frame->currentNode->data.function.params);
                    }
                    listFirst(frame->extra);
                    if (frame->extra->active != NULL){
                    nextFrame = prepFrame(&stack);
                    frame = readFrame(&stack);
                    nextFrame->currentNode = frame->extra->active->data;
                    pushFrame(&stack);
                    frame = nextFrame;
                    break;
                    }

                } else if (frame->childCTR == 0) {
                    appendF(&output, "defvar LF@_%s\npops LF@_%s\n",
                            retVal.value.retString, retVal.value.retString);

                    listNext(frame->extra);
                    if (frame->extra->active != NULL){
                        nextFrame = prepFrame(&stack);
                        frame = readFrame(&stack);
                        nextFrame->currentNode = *(Node **)frame->extra->active->data;
                        pushFrame(&stack);
                        frame = nextFrame;
                        break;
                    }

                }

                if(frame->childCTR == 0) {
                    frame->childCTR++;
                    STATIC_RECURSE(data.function.body);
                    break;
                } else {
                    appendF(&output, "pushs nil@nil\npopframe\nreturn\n");
                    listClean(frame->extra);
                    free(frame->extra);
                    frame->extra = NULL;
                    frame = popFrame(&stack);
                    break;

                }


            case AST_BLOCK:
                if(frame->new == true){
                    PREPLIST(block.statements);
                    frame->new = false;
                } else
                    listNext(&frame->currentNode->data.block.statements);

                if(frame->currentNode->data.block.statements.active == NULL){
                    frame = popFrame(&stack);
                } else{
                    RECURSE_LIST(block.statements);
                }
                break;

            //assings whatever comes back from assignment.value
            //to assignment.name
            case AST_ASSIGN:
                if(frame->new == true){
                    frame->new = false;
                    STATIC_RECURSE(data.assignment.value);
                    break;
                } else {
                    switch (retVal.type) {
                        case VAR:
                            appendF(&output, "move LF@_%s LF@_%s\n",
                                frame->currentNode->data.assignment.name,
                                retVal.value.retString);
                                break;

                        case TEMP:
                            appendF(&output, "move LF@_%s LF@T%ld\n",
                                frame->currentNode->data.assignment.name,
                                retVal.value.tempNum);
                            break;

                        case INT:
                            appendF(&output, "move LF@_%s int@%ld\n",
                                    frame->currentNode->data.assignment.name,
                                    retVal.value.tempNum);
                            break;

                        case REAL:
                            appendF(&output, "move LF@_%s float@%a\n",
                                    frame->currentNode->data.assignment.name,
                                    retVal.value.real);
                            break;
                        case STR:
                            appendF(&output, "move LF@_%s string@%s\n",
                                    frame->currentNode->data.assignment.name,
                                    retVal.value.retString);
                            break;
                        }
                    frame = popFrame(&stack);
                    break;
                }

            case AST_CALL:
                if(frame->new){
                    appendF(&output, "createframe\n");
                    frame->new = false;
                    listFirst(&frame->currentNode->data.call.args);
                }else {
                    listNext(&frame->currentNode->data.call.args);
                }



                if (frame->childCTR != 0){
                    switch (retVal.type) {
                        case VAR:
                            appendF(&output, "pushs LF@_%s\n",
                                    retVal.value.retString);
                            break;

                        case TEMP:
                            appendF(&output, "pushs LF@T%ld\n",
                                    retVal.value.tempNum);
                            break;

                        case INT:
                            appendF(&output, "pushs int@%ld\n",
                                    retVal.value.tempNum);
                            break;

                        case REAL:
                            appendF(&output, "pushs float@%a\n",
                                    retVal.value.real);
                            break;
                        case STR:
                            appendF(&output, "pushs string@%s\n",
                                    retVal.value.retString);
                            break;
                    }
                }

                if(frame->currentNode->data.call.args.active != NULL){
                    RECURSE_LIST(call.args);
                    break;
                }else{
                    frame->reserve = tempCTR++;
                    if(!findBuiltin(frame->currentNode->data.call.id)){
                        appendF(&output, "call $_%s\ndefvar LF@T%d\npops LF@T%d\n",
                                frame->currentNode->data.call.id,
                                frame->reserve, frame->reserve);

                    }else {
                        appendF(&output, "call $$%s\ndefvar LF@T%d\npops LF@T%d\n",
                            frame->currentNode->data.call.id,
                            frame->reserve, frame->reserve);

                    }
                    retVal.type = TEMP;
                    retVal.value.tempNum = frame->reserve;
                    frame = popFrame(&stack);
                    break;
                }

            case AST_VARDECL:
                if(frame->new){
                    frame->new = false;
                    appendF(&output, "defVar LF@_%s\n", frame->currentNode->data.varDeclaration.id);
                    STATIC_RECURSE(data.varDeclaration.initializer);
                    break;
                }else{
                    mode = frame->currentNode->data.varDeclaration.declType;
                    frame = popFrame(&stack);
                    break;
                }

            case AST_IDENTIFIER:
                retVal.type = VAR;
                retVal.value.retString = frame->currentNode->data.identifier.id;
                frame = popFrame(&stack);
                break;

            case AST_INT:
                retVal.value.tempNum = frame->currentNode->data.intLiteral.value;
                retVal.type = INT;
                mode = TYPE_INT;
                frame = popFrame(&stack);
                break;


            case AST_DOUBLE:
                retVal.value.real = frame->currentNode->data.doubleLiteral.value;
                retVal.type = REAL;
                mode = TYPE_DOUBLE;
                frame = popFrame(&stack);
                break;

            case AST_STRING:
                retVal.value.retString = frame->currentNode->data.stringLiteral.string;
                retVal.type = STR;
                mode = TYPE_STRING;
                frame = popFrame(&stack);
                break;


            case AST_BINARY:
                switch(frame->childCTR){

                    case 0:
                        frame->buffer = BSCtor();
                        frame->childCTR++;
                        STATIC_RECURSE(data.binary.left);
                        break;

                    case 1:

                        frame->reserve = tempCTR++;
                        appendF(&output, "defvar LF@T%d\n", frame->reserve);

                        switch (frame->currentNode->data.binary.op) {

                            case TOKEN_PLUS:
                                if (mode == TYPE_STRING)
                                    appendF(&frame->buffer, "concat ");
                                else
                                    appendF(&frame->buffer, "add ");
                                break;

                            case TOKEN_MINUS:
                                appendF(&frame->buffer, "sub ");
                                break;

                            case TOKEN_MUL:
                                appendF(&frame->buffer, "mul ");
                                break;

                            case TOKEN_DIV:
                                if(mode == TYPE_INT)
                                    appendF(&frame->buffer, "idiv ");
                                else
                                    appendF(&frame->buffer, "div ");

                                break;

                            case TOKEN_EQ:
                                mode = TYPE_UNSPECIFIED;
                                appendF(&frame->buffer, "eq ");
                                break;

                            case TOKEN_NEQ:
                                mode = TYPE_UNSPECIFIED;
                                appendF(&frame->buffer, "eq ");
                                break;

                            case TOKEN_GTE:
                            case TOKEN_LT:
                                mode = TYPE_UNSPECIFIED;
                                appendF(&frame->buffer, "lt ");
                                break;

                            case TOKEN_GT:
                            case TOKEN_LTE:
                                mode = TYPE_UNSPECIFIED;
                                appendF(&frame->buffer, "gt ");
                                break;

                            default:
                                exit(99);
                                break;
                        }

                        switch (retVal.type) {
                            case VAR:
                                appendF(&frame->buffer, "LF@T%d LF@_%s ",
                                        frame->reserve, retVal.value.retString);
                                break;

                            case INT:
                                appendF(&frame->buffer, "LF@T%d int@%ld ",
                                        frame->reserve, retVal.value.tempNum);
                                break;

                            case TEMP:
                                appendF(&frame->buffer, "LF@T%d LF@T%ld ",
                                        frame->reserve, retVal.value.tempNum);
                                break;

                            case REAL:
                                appendF(&frame->buffer, "LF@T%d float@%a ",
                                        frame->reserve, retVal.value.real);
                                break;

                            case STR:
                                appendF(&frame->buffer, "LF@T%d string@%s ",
                                        frame->reserve, retVal.value.retString);
                                break;

                        }

                    frame->childCTR++;
                    STATIC_RECURSE(data.binary.right);
                    break;

                    case 2:

                    printSymbol(retVal, &frame->buffer);

                    appendF(&output, "%s\n", frame->buffer.data);
                    BSDtor(&frame->buffer);

                    if(frame->currentNode->data.binary.op == TOKEN_NEQ){
                        appendF(&output, "defvar LF@T%d\nnot LF@T%d LF@T%d\n",
                                tempCTR, tempCTR, frame->reserve);
                        frame->reserve = tempCTR++;
                    }else if(frame->currentNode->data.binary.op == TOKEN_LTE) {
                        appendF(&output, "defvar LF@T%d\nnot LF@T%d LF@T%d\n",
                                tempCTR, tempCTR, frame->reserve);
                        frame->reserve = tempCTR++;
                    }else if(frame->currentNode->data.binary.op == TOKEN_GTE){
                        appendF(&output, "defvar LF@T%d\nnot LF@T%d LF@T%d\n",
                                tempCTR, tempCTR, frame->reserve);
                        frame->reserve = tempCTR++;
                    }

                    retVal.type = TEMP;
                    retVal.value.tempNum = frame->reserve;
                    frame = popFrame(&stack);

                }
                break;

            case AST_RETURN:
                if(frame->new == true && frame->currentNode->data.returnNode.value != NULL){
                    frame->new = false;
                    STATIC_RECURSE(data.returnNode.value);
                    break;
                }

                if(frame->currentNode->data.returnNode.value == NULL){
                    appendF(&output, "pushs nil@nil\n");
                } else {
                    switch (retVal.type) {
                        case VAR:
                            appendF(&output, "pushs LF@_%s\n", retVal.value.retString);
                            break;

                        case TEMP:
                            appendF(&output, "pushs LF@T%ld\n", retVal.value.tempNum);
                            break;

                        case INT:
                            appendF(&output, "pushs int@%ld\n", retVal.value.tempNum);
                            break;

                        case REAL:
                            appendF(&output, "pushs float@%a\n", retVal.value.real);
                            break;

                        case STR:
                            appendF(&output, "pushs string@%s\n", retVal.value.retString);
                            break;
                    }
                }

                appendF(&output, "popframe\nreturn\n");
                frame = popFrame(&stack);
                break;

            case AST_IF:

                switch (frame->childCTR) {
                    case 0:
                        frame->childCTR++;
                        frame->reserve = LCTR++;
                        STATIC_RECURSE(data.ifNode.cond);
                        break;

                    case 1:
                        frame->childCTR++;

                        switch (retVal.type) {
                            case VAR:
                                appendF(&output, "jumpifneq $else%d LF@_%s bool@true\n",
                                        frame->reserve, retVal.value.retString);
                                break;

                            case TEMP:
                                appendF(&output, "jumpifneq $else%d LF@T%ld bool@true\n",
                                        frame->reserve, retVal.value.tempNum);
                                break;

                            default:
                                handleError(NULL, 99, "invalid condition\n", 0);
                                break;

                        }

                        appendF(&output, "label $then%d\n", frame->reserve);
                        STATIC_RECURSE(data.ifNode.branchThen);
                        break;

                    case 2:
                        frame->childCTR++;
                        appendF(&output, "jump $brk%d\nlabel $else%d\n",
                                frame->reserve, frame->reserve);
                        if(frame->currentNode->data.ifNode.branchElse != NULL){
                            STATIC_RECURSE(data.ifNode.branchElse);
                        }
                        break;

                    case 3:
                        appendF(&output, "label $brk%d\n", frame->reserve);
                        frame = popFrame(&stack);
                        break;
                    }
                break;

            case AST_WHILE:
                switch(frame->childCTR){
                    case 0:
                        frame->childCTR++;
                        frame->reserve = LCTR++;
                        appendF(&output, "label $L%d\n", frame->reserve);
                        STATIC_RECURSE(data.whileNode.cond);
                        break;

                    case 1:
                        switch (retVal.type) {
                            case VAR:
                                appendF(&output, "jumpifneq $brk%d LF@_%s bool@true\n",
                                        frame->reserve, retVal.value.retString);
                                break;

                            case TEMP:
                                appendF(&output, "jumpifneq $brk%d LF@T%d bool@true\n",
                                        frame->reserve, retVal.value.tempNum);
                                break;

                            default:
                                handleError(NULL, 99, "invalid condition\n", 0);
                                break;

                        }
                        frame->childCTR++;
                        STATIC_RECURSE(data.whileNode.body);
                        break;

                    case 2:
                        appendF(&output, "jump $L%d\nlabel $brk%d\n",
                                frame->reserve, frame->reserve);

                        frame = popFrame(&stack);
                        break;

                }

                break;

            case AST_PARAM:
                retVal.type = VAR;
                retVal.value.retString = frame->currentNode->data.parameter.id;
                frame = popFrame(&stack);
                break;

            case AST_UNARY:

                if(frame->new){
                    frame->new = false;
                    STATIC_RECURSE(data.unary.operand);
                    break;
                }

                frame->reserve = tempCTR++;

                switch (frame->currentNode->data.unary.op) {
                    case TOKEN_PLUS:

                        break;

                    case TOKEN_MINUS:
                        appendF(&output, "defvar LF@T%d\n", frame->reserve);
                        if(mode == TYPE_DOUBLE){

                            switch (retVal.type) {
                                case VAR:
                                    appendF(&output, "mul LF@T%d LF@_%s float@-0x1p+0\n",
                                            frame->reserve, retVal.value.retString);
                                    break;

                                case TEMP:
                                    appendF(&output, "mul LF@T%d LF@T%d float@-0x1p+0\n",
                                            frame->reserve, retVal.value.tempNum);
                                    break;

                                case REAL:
                                    appendF(&output, "mul LF@T%d float@%a float@-0x1p+0\n",
                                            frame->reserve, retVal.value.real);
                                    break;

                                default:

                                    break;
                            }
                        }else{

                            switch (retVal.type) {
                                case VAR:
                                    appendF(&output, "mul LF@T%d LF@_%s int@-1\n",
                                            frame->reserve, retVal.value.retString);
                                    break;

                                case INT:
                                    appendF(&output, "mul LF@T%d int@%d int@-1\n",
                                            frame->reserve, retVal.value.tempNum);
                                    break;

                                case TEMP:
                                    appendF(&output, "mul LF@T%d LF@T%d int@-1\n",
                                            frame->reserve, retVal.value.tempNum);
                                    break;

                                default:

                                    break;
                            }


                            retVal.type = TEMP;
                            retVal.value.tempNum = frame->reserve;

                        }

                        break;

                    default:
                        handleError(NULL, 99, "unexpected unary operand\n", 0);
                        break;
                    }


                frame = popFrame(&stack);
                break;

            case AST_UNIT:
                handleError(NULL, 99, "unit in generator\n", 0);
                break;
        }
    }

    done:

    stackDtor(&stack);

    return output.data;
}
