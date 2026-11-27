/**
 * @file gen.c
 * @brief Code generator for IFJ26 -> IFJcode26
 *
 * @authors
 * - Josef Kalousek - 279279
 * - Tadeáš Vykopal - xvykopt00
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gen.h"
#include "error.h"

#define DEFAULTSIZE 512

// ==================== betterString_t ====================

typedef struct {
    char *data;
    uint32_t top;       // index of first NULL byte
    uint32_t capacity;  // amount of allocated data units
} betterString_t;

static inline betterString_t BSCtor(void) {
    betterString_t string;
    string.data = calloc(DEFAULTSIZE, sizeof(char));
    if (string.data == NULL) exit(99);
    string.capacity = DEFAULTSIZE;
    string.top = 0;
    return string;
}

static inline void appendF(betterString_t *dst, const char *src, ...) {
    va_list args, copy;
    va_start(args, src);
    va_copy(copy, args);
    int32_t expLen = vsnprintf(NULL, 0, src, args);
    if (expLen < 0) exit(99);

    char *expStr = malloc((size_t)(expLen + 1) * sizeof(char));
    if (expStr == NULL) exit(99);

    vsnprintf(expStr, (size_t)(expLen + 1), src, copy);
    va_end(args);
    va_end(copy);

    while (dst->top + (uint32_t)expLen + 1 >= dst->capacity) {
        void *temp = realloc(dst->data, 2 * dst->capacity * sizeof(char));
        if (temp == NULL) exit(99);
        dst->data = temp;
        memset(dst->data + dst->capacity, 0, dst->capacity);
        dst->capacity *= 2;
    }

    for (int32_t i = 0; i < expLen; i++) {
        dst->data[dst->top++] = expStr[i];
    }
    dst->data[dst->top] = '\0';

    free(expStr);
}

static inline void BSDtor(betterString_t *string) {
    free(string->data);
    string->capacity = 0;
    string->top = 0;
    string->data = NULL;
}

static void insertAt(betterString_t *dst, uint32_t pos, const char *ins) {
    uint32_t len = (uint32_t)strlen(ins);
    if (len == 0) return;
    while (dst->top + len + 1 >= dst->capacity) {
        void *temp = realloc(dst->data, 2 * dst->capacity * sizeof(char));
        if (temp == NULL) exit(99);
        dst->data = temp;
        memset(dst->data + dst->capacity, 0, dst->capacity);
        dst->capacity *= 2;
    }
    memmove(dst->data + pos + len, dst->data + pos, dst->top - pos + 1);
    memcpy(dst->data + pos, ins, len);
    dst->top += len;
}

// ==================== Value & Stack ====================

typedef struct {
    enum {
        INT,
        TEMP,
        REAL,
        STR,
        VAR,
        NIL
    } type;
    DeclType dataType;
    union {
        int64_t tempNum;
        double real;
        char *retString;
    } value;
} retVal_t;

typedef struct {
    retVal_t left;
    Node *currentNode;
    List *extra;
    uint32_t childCTR;
    uint32_t reserve;
    bool new;
} frame_t;

typedef struct {
    frame_t *data;
    uint32_t top;
    uint32_t capacity;
} stack_t;

static inline stack_t stackCtor(void) {
    stack_t stack;
    stack.data = malloc(DEFAULTSIZE * sizeof(frame_t));
    if (stack.data == NULL) exit(99);
    stack.top = 0;
    stack.capacity = DEFAULTSIZE;
    return stack;
}

static inline frame_t *prepFrame(stack_t *stack) {
    if (stack->top + 1 >= stack->capacity) {
        void *temp = realloc(stack->data, 2 * stack->capacity * sizeof(frame_t));
        if (temp == NULL) exit(99);
        stack->data = temp;
        stack->capacity *= 2;
    }
    memset(&stack->data[stack->top + 1], 0, sizeof(frame_t));
    stack->data[stack->top + 1].new = true;
    return &stack->data[stack->top + 1];
}

static inline void pushFrame(stack_t *stack) {
    stack->top++;
}

static inline frame_t *popFrame(stack_t *stack) {
    if (stack->top == 0) return NULL;
    return &stack->data[--stack->top];
}

static inline frame_t *readFrame(stack_t *stack) {
    return &stack->data[stack->top];
}

static inline void stackDtor(stack_t *stack) {
    free(stack->data);
    stack->capacity = 0;
    stack->top = 0;
    stack->data = NULL;
}

// ==================== Type & String Helpers ====================

typedef struct {
    const char *name;
    DeclType type;
} varType_t;

static DeclType getVarType(const varType_t *vars, uint32_t count, const char *name) {
    for (int i = (int)count - 1; i >= 0; i--) {
        if (!strcmp(vars[i].name, name)) return vars[i].type;
    }
    return TYPE_UNSPECIFIED;
}

// paramIdx < 0 returns function returnType; paramIdx >= 0 returns that parameter's DeclType
static DeclType getFuncType(Node *const *funcs, uint32_t funcCount, const char *fnName, int paramIdx) {
    if (!strcmp(fnName, "print"))      return (paramIdx < 0) ? TYPE_UNIT   : TYPE_ANY;
    if (!strcmp(fnName, "readLine"))   return TYPE_STRING;
    if (!strcmp(fnName, "readInt"))    return TYPE_INT;
    if (!strcmp(fnName, "readDouble")) return TYPE_DOUBLE;
    if (!strcmp(fnName, "toStr"))      return (paramIdx < 0) ? TYPE_STRING : TYPE_ANY;
    if (!strcmp(fnName, "toInt"))      return (paramIdx < 0) ? TYPE_INT    : TYPE_DOUBLE;
    if (!strcmp(fnName, "strLength"))  return (paramIdx < 0) ? TYPE_INT    : TYPE_STRING;
    if (!strcmp(fnName, "substr"))     return (paramIdx < 0) ? TYPE_STRING : (paramIdx == 0 ? TYPE_STRING : TYPE_INT);
    if (!strcmp(fnName, "ord"))        return (paramIdx < 0) ? TYPE_INT    : (paramIdx == 0 ? TYPE_STRING : TYPE_INT);
    if (!strcmp(fnName, "chr"))        return (paramIdx < 0) ? TYPE_STRING : TYPE_INT;

    for (uint32_t i = 0; i < funcCount; i++) {
        Node *fn = funcs[i];
        if (!strcmp(fn->data.function.id, fnName)) {
            if (paramIdx < 0) return fn->data.function.returnType;
            int idx = 0;
            listFirst(&fn->data.function.params);
            while (fn->data.function.params.active != NULL) {
                if (idx == paramIdx) {
                    return (*(Node **)fn->data.function.params.active->data)->data.parameter.declType;
                }
                idx++;
                listNext(&fn->data.function.params);
            }
            return TYPE_UNSPECIFIED;
        }
    }
    return TYPE_UNSPECIFIED;
}

static void appendEscapedStr(betterString_t *dst, const char *s) {
    appendF(dst, "string@");
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        unsigned char ch = *p;
        if (ch == '\\' && *(p + 1) != '\0') {
            p++;
            switch (*p) {
                case 'n':  ch = '\n'; break;
                case 'r':  ch = '\r'; break;
                case 't':  ch = '\t'; break;
                case '"':  ch = '"';  break;
                case '\\': ch = '\\'; break;
                default:   ch = *p;   break;
            }
        }
        if (ch <= 32 || ch == '#' || ch == '\\') {
            appendF(dst, "\\%03d", (int)ch);
        } else {
            appendF(dst, "%c", (char)ch);
        }
    }
}

void printSymbol(retVal_t retVal, betterString_t *string) {
    switch (retVal.type) {
        case VAR:
            appendF(string, "LF@_%s", retVal.value.retString);
            break;
        case INT:
            appendF(string, "int@%ld", (long)retVal.value.tempNum);
            break;
        case TEMP:
            appendF(string, "LF@T%ld", (long)retVal.value.tempNum);
            break;
        case REAL:
            appendF(string, "float@%a", retVal.value.real);
            break;
        case STR:
            appendEscapedStr(string, retVal.value.retString);
            break;
        case NIL:
            appendF(string, "nil@nil");
            break;
    }
}

static void promoteToDouble(retVal_t *val, betterString_t *fnDefs, betterString_t *out, uint64_t *tempCTR) {
    if (val->dataType != TYPE_INT) return;
    uint64_t t = (*tempCTR)++;
    appendF(fnDefs, "defvar LF@T%lu\n", (unsigned long)t);
    appendF(out, "int2float LF@T%lu ", (unsigned long)t);
    printSymbol(*val, out);
    appendF(out, "\n");
    val->type = TEMP;
    val->value.tempNum = (int64_t)t;
    val->dataType = TYPE_DOUBLE;
}

// ==================== Recursion Macros ====================

#define STATIC_RECURSE(child)                           \
    nextFrame = prepFrame(&stack);                      \
    frame = readFrame(&stack);                          \
    nextFrame->currentNode = frame->currentNode->child; \
    pushFrame(&stack);                                  \
    frame = nextFrame

#define RECURSE_LIST(type)                                                            \
    nextFrame = prepFrame(&stack);                                                    \
    frame = readFrame(&stack);                                                        \
    nextFrame->currentNode = *(Node **)frame->currentNode->data.type.active->data;    \
    frame->childCTR++;                                                                \
    pushFrame(&stack);                                                                \
    frame = nextFrame

#define PREPLIST(type) \
    listFirst(&frame->currentNode->data.type)

// ==================== Built-in Functions ====================

static const char *builtinCode =
//print(value: Any): Unit
"label $$print\n"
"pushframe\n"
"defvar LF@v\n"
"pops LF@v\n"
"defvar LF@t\n"
"type LF@t LF@v\n"
"jumpifeq $$print_nil LF@t string@nil\n"
"write LF@v\n"
"jump $$print_end\n"
"label $$print_nil\n"
"write string@()\n"
"label $$print_end\n"
"pushs nil@nil\n"
"popframe\n"
"return\n"

//readLine(): String
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

//readInt(): Int
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

//readDouble(): Double
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

//toStr(value: Any): String
"label $$toStr\n"
"pushframe\n"
"defvar LF@v\n"
"pops LF@v\n"
"defvar LF@t\n"
"type LF@t LF@v\n"
"jumpifeq $$toStr_int LF@t string@int\n"
"jumpifeq $$toStr_float LF@t string@float\n"
"jumpifeq $$toStr_bool LF@t string@bool\n"
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
"label $$toStr_bool\n"
"jumpifeq $$toStr_true LF@v bool@true\n"
"pushs string@false\n"
"jump $$toStr_end\n"
"label $$toStr_true\n"
"pushs string@true\n"
"jump $$toStr_end\n"
"label $$toStr_nil\n"
"pushs string@()\n"
"label $$toStr_end\n"
"popframe\n"
"return\n"

//toInt(value: Double): Int
"label $$toInt\n"
"pushframe\n"
"defvar LF@v\n"
"pops LF@v\n"
"float2int LF@v LF@v\n"
"pushs LF@v\n"
"popframe\n"
"return\n"

//strLength(value: String): Int
"label $$strLength\n"
"pushframe\n"
"defvar LF@v\n"
"pops LF@v\n"
"strlen LF@v LF@v\n"
"pushs LF@v\n"
"popframe\n"
"return\n"

//substr(value: String, start: Int, end: Int): String
"label $$substr\n"
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
"jumpifeq $$substr_err LF@c bool@true\n"
"lt LF@c LF@e int@0\n"
"jumpifeq $$substr_err LF@c bool@true\n"
"gt LF@c LF@b LF@e\n"
"jumpifeq $$substr_err LF@c bool@true\n"
"lt LF@c LF@b LF@len\n"
"jumpifeq $$substr_err LF@c bool@false\n"
"gt LF@c LF@e LF@len\n"
"jumpifeq $$substr_err LF@c bool@true\n"
"defvar LF@r\n"
"move LF@r string@\n"
"defvar LF@ch\n"
"label $$substr_loop\n"
"jumpifeq $$substr_end LF@b LF@e\n"
"getchar LF@ch LF@s LF@b\n"
"concat LF@r LF@r LF@ch\n"
"add LF@b LF@b int@1\n"
"jump $$substr_loop\n"
"label $$substr_end\n"
"pushs LF@r\n"
"popframe\n"
"return\n"
"label $$substr_err\n"
"exit int@40\n"

//ord(value: String, index: Int): Int
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

//chr(code: Int): String
"label $$chr\n"
"pushframe\n"
"defvar LF@v\n"
"pops LF@v\n"
"int2char LF@v LF@v\n"
"pushs LF@v\n"
"popframe\n"
"return\n";

int findBuiltin(char *id) {
    static const char *builtinNames[] = {
        "print",
        "readLine",
        "readInt",
        "readDouble",
        "toStr",
        "toInt",
        "strLength",
        "substr",
        "ord",
        "chr",
    };

    for (int i = 0; i < 10; i++) {
        if (!strcmp(builtinNames[i], id)) return i + 1;
    }
    return 0;
}

// ==================== Main Generator ====================

char *codeGen(Node top, Error *err) {
    static uint64_t LCTR = 0;
    static uint64_t tempCTR = 0;

    retVal_t retVal = {0};
    DeclType currentFnRetType = TYPE_UNSPECIFIED;

    varType_t vars[512];
    uint32_t varCount = 0;

    Node *funcs[512];
    uint32_t funcCount = 0;

    stack_t stack = stackCtor();
    frame_t *frame = prepFrame(&stack);
    pushFrame(&stack);
    frame_t *nextFrame;

    betterString_t output = BSCtor();
    betterString_t fnDefs = BSCtor();
    uint32_t fnStart = 0;

    frame->currentNode = &top;

    while (true) {
        switch (frame->currentNode->type) {
            case AST_PROGRAM:
                if (frame->new == true) {
                    frame->new = false;
                    appendF(&output, ".IFJcode26\n");

                    const char *mainId = NULL;
                    PREPLIST(program.declarations);
                    while (frame->currentNode->data.program.declarations.active != NULL) {
                        Node *decl = *(Node **)frame->currentNode->data.program.declarations.active->data;
                        if (decl->type == AST_FUNCTION) {
                            if (funcCount < 512) {
                                funcs[funcCount++] = decl;
                            }
                            if (decl->data.function.isMain) {
                                mainId = decl->data.function.id;
                            }
                        }
                        listNext(&frame->currentNode->data.program.declarations);
                    }

                    if (mainId != NULL) {
                        appendF(&output, "createframe\ncall $_%s\nexit int@0\n", mainId);
                        appendF(&output, "%s", builtinCode);
                    }
                    PREPLIST(program.declarations);
                } else {
                    listNext(&frame->currentNode->data.program.declarations);
                }

                if (frame->currentNode->data.program.declarations.active == NULL) {
                    goto done;
                }

                RECURSE_LIST(program.declarations);
                break;

            case AST_FUNCTION:
                if (frame->new == true) {
                    varCount = 0;
                    currentFnRetType = frame->currentNode->data.function.returnType;
                    appendF(&output, "label $_%s\npushframe\n", frame->currentNode->data.function.id);
                    fnStart = output.top;
                    fnDefs.top = 0;
                    fnDefs.data[0] = '\0';
                    frame->new = false;

                    PREPLIST(function.params);
                    frame->extra = malloc(sizeof(List));
                    if (frame->extra == NULL) {
                        handleError(err, 99, "failed allocation\n", 0);
                    }
                    listInit(frame->extra, (DT){sizeof(Node *), NULL});

                    while (frame->currentNode->data.function.params.active != NULL) {
                        if (!insertFirst(frame->extra, frame->currentNode->data.function.params.active->data)) {
                            handleError(err, 99, "failed making reverse list\n", 0);
                        }
                        listNext(&frame->currentNode->data.function.params);
                    }
                    listFirst(frame->extra);
                    if (frame->extra->active != NULL) {
                        nextFrame = prepFrame(&stack);
                        frame = readFrame(&stack);
                        nextFrame->currentNode = *(Node **)frame->extra->active->data;
                        pushFrame(&stack);
                        frame = nextFrame;
                        break;
                    }
                } else if (frame->childCTR == 0) {
                    if (varCount < 512) {
                        vars[varCount++] = (varType_t){ retVal.value.retString, retVal.dataType };
                    }
                    appendF(&fnDefs, "defvar LF@_%s\n", retVal.value.retString);
                    appendF(&output, "pops LF@_%s\n", retVal.value.retString);

                    listNext(frame->extra);
                    if (frame->extra->active != NULL) {
                        nextFrame = prepFrame(&stack);
                        frame = readFrame(&stack);
                        nextFrame->currentNode = *(Node **)frame->extra->active->data;
                        pushFrame(&stack);
                        frame = nextFrame;
                        break;
                    }
                }

                if (frame->childCTR == 0) {
                    frame->childCTR++;
                    retVal.type = NIL;
                    retVal.dataType = TYPE_UNIT;
                    STATIC_RECURSE(data.function.body);
                    break;
                } else {
                    if (currentFnRetType == TYPE_UNIT) {
                        appendF(&output, "pushs nil@nil\n");
                    } else {
                        if (currentFnRetType == TYPE_DOUBLE) {
                            promoteToDouble(&retVal, &fnDefs, &output, &tempCTR);
                        }
                        appendF(&output, "pushs ");
                        printSymbol(retVal, &output);
                        appendF(&output, "\n");
                    }
                    appendF(&output, "popframe\nreturn\n");
                    insertAt(&output, fnStart, fnDefs.data);

                    listClean(frame->extra);
                    free(frame->extra);
                    frame->extra = NULL;
                    frame = popFrame(&stack);
                    break;
                }

            case AST_BLOCK:
                if (frame->new == true) {
                    frame->reserve = varCount;
                    PREPLIST(block.statements);
                    frame->new = false;
                } else {
                    listNext(&frame->currentNode->data.block.statements);
                }

                if (frame->currentNode->data.block.statements.active == NULL) {
                    varCount = frame->reserve;
                    frame = popFrame(&stack);
                } else {
                    RECURSE_LIST(block.statements);
                }
                break;

            case AST_VARDECL:
                if (frame->new) {
                    frame->new = false;
                    char *id = frame->currentNode->data.varDeclaration.id;
                    char defLine[256];
                    snprintf(defLine, sizeof(defLine), "defvar LF@_%s\n", id);
                    if (strstr(fnDefs.data, defLine) == NULL) {
                        appendF(&fnDefs, "%s", defLine);
                    }
                    STATIC_RECURSE(data.varDeclaration.initializer);
                    break;
                } else {
                    DeclType t = frame->currentNode->data.varDeclaration.declType;
                    if (t == TYPE_UNSPECIFIED) {
                        t = retVal.dataType;
                    }
                    if (varCount < 512) {
                        vars[varCount++] = (varType_t){ frame->currentNode->data.varDeclaration.id, t };
                    }
                    if (t == TYPE_DOUBLE) {
                        promoteToDouble(&retVal, &fnDefs, &output, &tempCTR);
                    }
                    appendF(&output, "move LF@_%s ", frame->currentNode->data.varDeclaration.id);
                    printSymbol(retVal, &output);
                    appendF(&output, "\n");

                    retVal.type = NIL;
                    retVal.dataType = TYPE_UNIT;
                    frame = popFrame(&stack);
                    break;
                }

            case AST_ASSIGN:
                if (frame->new == true) {
                    frame->new = false;
                    STATIC_RECURSE(data.assignment.value);
                    break;
                } else {
                    DeclType dstType = getVarType(vars, varCount, frame->currentNode->data.assignment.name);
                    if (dstType == TYPE_DOUBLE) {
                        promoteToDouble(&retVal, &fnDefs, &output, &tempCTR);
                    }
                    appendF(&output, "move LF@_%s ", frame->currentNode->data.assignment.name);
                    printSymbol(retVal, &output);
                    appendF(&output, "\n");

                    retVal.type = NIL;
                    retVal.dataType = TYPE_UNIT;
                    frame = popFrame(&stack);
                    break;
                }

            case AST_CALL:
                if (frame->new) {
                    appendF(&output, "createframe\n");
                    frame->new = false;
                    listFirst(&frame->currentNode->data.call.args);
                } else {
                    listNext(&frame->currentNode->data.call.args);
                }

                if (frame->childCTR != 0) {
                    DeclType paramType = getFuncType(funcs, funcCount, frame->currentNode->data.call.id, (int)frame->childCTR - 1);
                    if (paramType == TYPE_DOUBLE) {
                        promoteToDouble(&retVal, &fnDefs, &output, &tempCTR);
                    }
                    appendF(&output, "pushs ");
                    printSymbol(retVal, &output);
                    appendF(&output, "\n");
                }

                if (frame->currentNode->data.call.args.active != NULL) {
                    RECURSE_LIST(call.args);
                    break;
                } else {
                    frame->reserve = (uint32_t)(tempCTR++);
                    appendF(&fnDefs, "defvar LF@T%u\n", frame->reserve);
                    if (!findBuiltin(frame->currentNode->data.call.id)) {
                        appendF(&output, "call $_%s\npops LF@T%u\n",
                                frame->currentNode->data.call.id, frame->reserve);
                    } else {
                        appendF(&output, "call $$%s\npops LF@T%u\n",
                                frame->currentNode->data.call.id, frame->reserve);
                    }
                    retVal.type = TEMP;
                    retVal.value.tempNum = (int64_t)frame->reserve;
                    retVal.dataType = getFuncType(funcs, funcCount, frame->currentNode->data.call.id, -1);
                    frame = popFrame(&stack);
                    break;
                }

            case AST_IDENTIFIER:
                retVal.type = VAR;
                retVal.value.retString = frame->currentNode->data.identifier.id;
                retVal.dataType = getVarType(vars, varCount, retVal.value.retString);
                frame = popFrame(&stack);
                break;

            case AST_PARAM:
                retVal.type = VAR;
                retVal.value.retString = frame->currentNode->data.parameter.id;
                retVal.dataType = frame->currentNode->data.parameter.declType;
                frame = popFrame(&stack);
                break;

            case AST_INT:
                retVal.type = INT;
                retVal.value.tempNum = frame->currentNode->data.intLiteral.value;
                retVal.dataType = TYPE_INT;
                frame = popFrame(&stack);
                break;

            case AST_DOUBLE:
                retVal.type = REAL;
                retVal.value.real = frame->currentNode->data.doubleLiteral.value;
                retVal.dataType = TYPE_DOUBLE;
                frame = popFrame(&stack);
                break;

            case AST_STRING:
                retVal.type = STR;
                retVal.value.retString = frame->currentNode->data.stringLiteral.string;
                retVal.dataType = TYPE_STRING;
                frame = popFrame(&stack);
                break;

            case AST_UNIT:
                retVal.type = NIL;
                retVal.dataType = TYPE_UNIT;
                frame = popFrame(&stack);
                break;

            case AST_BINARY:
                switch (frame->childCTR) {
                    case 0:
                        frame->childCTR++;
                        STATIC_RECURSE(data.binary.left);
                        break;

                    case 1:
                        frame->left = retVal;
                        frame->childCTR++;
                        STATIC_RECURSE(data.binary.right);
                        break;

                    case 2: {
                        TokenType op = frame->currentNode->data.binary.op;

                        if (frame->left.dataType == TYPE_DOUBLE || retVal.dataType == TYPE_DOUBLE) {
                            promoteToDouble(&frame->left, &fnDefs, &output, &tempCTR);
                            promoteToDouble(&retVal, &fnDefs, &output, &tempCTR);
                        }

                        frame->reserve = (uint32_t)(tempCTR++);
                        appendF(&fnDefs, "defvar LF@T%u\n", frame->reserve);

                        const char *instr = NULL;
                        switch (op) {
                            case TOKEN_PLUS:  instr = (frame->left.dataType == TYPE_STRING) ? "concat" : "add"; break;
                            case TOKEN_MINUS: instr = "sub"; break;
                            case TOKEN_MUL:   instr = "mul"; break;
                            case TOKEN_DIV:   instr = (frame->left.dataType == TYPE_INT) ? "idiv" : "div"; break;
                            case TOKEN_EQ:
                            case TOKEN_NEQ:   instr = "eq"; break;
                            case TOKEN_LT:
                            case TOKEN_GTE:   instr = "lt"; break;
                            case TOKEN_GT:
                            case TOKEN_LTE:   instr = "gt"; break;
                            default:          exit(99);
                        }

                        appendF(&output, "%s LF@T%u ", instr, frame->reserve);
                        printSymbol(frame->left, &output);
                        appendF(&output, " ");
                        printSymbol(retVal, &output);
                        appendF(&output, "\n");

                        if (op == TOKEN_NEQ || op == TOKEN_LTE || op == TOKEN_GTE) {
                            appendF(&output, "not LF@T%u LF@T%u\n", frame->reserve, frame->reserve);
                        }

                        retVal.type = TEMP;
                        retVal.value.tempNum = (int64_t)frame->reserve;
                        retVal.dataType = (op == TOKEN_EQ || op == TOKEN_NEQ || op == TOKEN_LT ||
                                           op == TOKEN_LTE || op == TOKEN_GT || op == TOKEN_GTE)
                                          ? TYPE_BOOL : frame->left.dataType;
                        frame = popFrame(&stack);
                        break;
                    }
                }
                break;

            case AST_UNARY:
                if (frame->new) {
                    frame->new = false;
                    STATIC_RECURSE(data.unary.operand);
                    break;
                }

                switch (frame->currentNode->data.unary.op) {
                    case TOKEN_PLUS:
                        break;

                    case TOKEN_MINUS:
                        frame->reserve = (uint32_t)(tempCTR++);
                        appendF(&fnDefs, "defvar LF@T%u\n", frame->reserve);
                        appendF(&output, "mul LF@T%u ", frame->reserve);
                        printSymbol(retVal, &output);
                        if (retVal.dataType == TYPE_DOUBLE) {
                            appendF(&output, " float@-0x1p+0\n");
                        } else {
                            appendF(&output, " int@-1\n");
                        }
                        retVal.type = TEMP;
                        retVal.value.tempNum = (int64_t)frame->reserve;
                        break;

                    default:
                        handleError(err, 99, "unexpected unary operand\n", 0);
                        break;
                }

                frame = popFrame(&stack);
                break;

            case AST_RETURN:
                if (frame->new == true && frame->currentNode->data.returnNode.value != NULL) {
                    frame->new = false;
                    STATIC_RECURSE(data.returnNode.value);
                    break;
                }

                if (frame->currentNode->data.returnNode.value == NULL || currentFnRetType == TYPE_UNIT) {
                    appendF(&output, "pushs nil@nil\n");
                } else {
                    if (currentFnRetType == TYPE_DOUBLE) {
                        promoteToDouble(&retVal, &fnDefs, &output, &tempCTR);
                    }
                    appendF(&output, "pushs ");
                    printSymbol(retVal, &output);
                    appendF(&output, "\n");
                }

                appendF(&output, "popframe\nreturn\n");
                frame = popFrame(&stack);
                break;

            case AST_IF:
                switch (frame->childCTR) {
                    case 0:
                        frame->childCTR++;
                        frame->reserve = (uint32_t)(LCTR++);
                        STATIC_RECURSE(data.ifNode.cond);
                        break;

                    case 1:
                        frame->childCTR++;
                        appendF(&output, "jumpifneq $else%u ", frame->reserve);
                        printSymbol(retVal, &output);
                        appendF(&output, " bool@true\nlabel $then%u\n", frame->reserve);
                        STATIC_RECURSE(data.ifNode.branchThen);
                        break;

                    case 2:
                        frame->childCTR++;
                        appendF(&output, "jump $brk%u\nlabel $else%u\n",
                                frame->reserve, frame->reserve);
                        if (frame->currentNode->data.ifNode.branchElse != NULL) {
                            STATIC_RECURSE(data.ifNode.branchElse);
                        }
                        break;

                    case 3:
                        appendF(&output, "label $brk%u\n", frame->reserve);
                        retVal.type = NIL;
                        retVal.dataType = TYPE_UNIT;
                        frame = popFrame(&stack);
                        break;
                }
                break;

            case AST_WHILE:
                switch (frame->childCTR) {
                    case 0:
                        frame->childCTR++;
                        frame->reserve = (uint32_t)(LCTR++);
                        appendF(&output, "label $L%u\n", frame->reserve);
                        STATIC_RECURSE(data.whileNode.cond);
                        break;

                    case 1:
                        appendF(&output, "jumpifneq $brk%u ", frame->reserve);
                        printSymbol(retVal, &output);
                        appendF(&output, " bool@true\n");
                        frame->childCTR++;
                        STATIC_RECURSE(data.whileNode.body);
                        break;

                    case 2:
                        appendF(&output, "jump $L%u\nlabel $brk%u\n",
                                frame->reserve, frame->reserve);
                        retVal.type = NIL;
                        retVal.dataType = TYPE_UNIT;
                        frame = popFrame(&stack);
                        break;
                }
                break;
        }
    }

done:
    BSDtor(&fnDefs);
    stackDtor(&stack);
    return output.data;
}
