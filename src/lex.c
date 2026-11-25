/**
* @file lex.c
* @brief Implementation of lexical analyzer for the language IFJ26
*
* This module reads input from stdin, manages indentation and generates a stream of tokens for the syntax analyzer.
*
* @authors
* - Tadeáš Vykopal (xvykopt00)
*
* @date 2026
*/

#define _POSIX_C_SOURCE 200809L //so the strdup works if we use C99
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lex.h"

#define MAX_INDENT_STACK 1024

//counters
static int lineCounter = 1;
static int spaceCounter = 0;
static int parenDepth = 0;
static int owedDedents = 0;
static int currentLogicalLineIndent = 0;

//flags
static int isStartOfLine = 1;

//indentation stack
static int indentStack[MAX_INDENT_STACK] = {0};
static int stackTop = 0;

/**
 * @brief Internal states of the lexer's finite state machine.
 */
typedef enum {
    START,           /**< Initial state, expects the beginning of a new lexeme */
    LINE_START,      /**< Evaluates leading whitespace for indentation logic */

    IDENTIFIER,      /**< Reading an identifier or keyword */

    INT,             /**< Reading an integer literal */
    DOUBLE,          /**< Reading the fractional part of a double literal */
    DOUBLE_EXP,      /**< Reading the exponent part of a double literal */
    
    STRING,          /**< Reading a standard string literal */
    STRING_ESC,      /**< Reading an escape sequence inside a string */
    BLOCK_STRING,    /**< Reading a multiline block string literal */
} LexerState;

/**
* @brief Struct of the dynamically allocated text buffer for loading lexems
*/
typedef struct {
    char *text;
    int length;
    int capacity;
} StringBuffer;

static inline int isAsciiAlpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline int isAsciiDigit(int c) {
    return (c >= '0' && c <= '9');
}

static inline int isAsciiAlnum(int c) {
    return isAsciiAlpha(c) || isAsciiDigit(c);
}

static inline int isHorizSpace(int c) {
    return c == ' ' || c == '\t' || c == '\r';
}

/**
* @brief Adds a char to the dynamic buffer and allocates more memory if it's needed.
* 
* @param buffer Pointer to the initialized struct StringBuffer
* @param c The char to add (obtained using getchar())
* @param err
*
* @return 0 on error, 1 on success
*
* @warning You MUST check the return value of this function! If it returns 0, a fatal INTERNAL_ERROR has occurred and the caller must exit immediately.
* 
* @attention The err pointer must already be allocated before calling this function.
* 
* @note If the buffer text was NULL, it initially allocates 64 bytes.
*/
static int appendChar(StringBuffer *buffer, int c, Error *err) {
    if (buffer->text == NULL) {
        buffer->capacity = 64;
        buffer->text = malloc((size_t)buffer->capacity);
        if (buffer->text == NULL) {
            handleError(err, INTERNAL_ERROR, "Memory allocation failed", lineCounter);
            return 0;
        }
    } else if (buffer->length + 1 >= buffer->capacity) {
        buffer->capacity *= 2;
        char *newText = realloc(buffer->text, (size_t)buffer->capacity);
        if (newText == NULL) {
            free(buffer->text);
            buffer->text = NULL;
            handleError(err, INTERNAL_ERROR, "Memory reallocation failed", lineCounter);
            return 0;
        }
        buffer->text = newText;
    }
    buffer->text[buffer->length++] = (char) c;
    buffer->text[buffer->length] = '\0';
    return 1;
}

/**
* @brief Finds out if the loaded string is a keyword or an identifier.
*
* Utilizes string length to make the lookup faster
*
* @param text Pointer to the loaded string
* @param len Length of the string
* @param err Pointer to the shared error structure
* 
* @return Token type
*/
static TokenType findKeyword(const char *text, const int len, Error *err) {
    switch (len) {
        case 1:
            if (text[0] == '_') {
                handleError(err, LEX_ERROR, "'_' itself cannot be an identifier", lineCounter);
                return TOKEN_EOF;
            }
            break;
        case 2:
            if (text[0] == 'd' && text[1] == 'o') return TOKEN_KW_DO;
            if (text[0] == 'i' && text[1] == 'f') return TOKEN_KW_IF;
            break;
        case 3:
            if (strcmp(text, "def") == 0) return TOKEN_KW_DEF;
            if (strcmp(text, "val") == 0) return TOKEN_KW_VAL;
            if (strcmp(text, "var") == 0) return TOKEN_KW_VAR;
            if (strcmp(text, "Int") == 0) return TOKEN_TYPE_INT;
            break;
        case 4:
            if (strcmp(text, "else") == 0) return TOKEN_KW_ELSE;
            if (strcmp(text, "then") == 0) return TOKEN_KW_THEN;
            if (strcmp(text, "Unit") == 0) return TOKEN_TYPE_UNIT;
            break;
        case 5:
            if (strcmp(text, "while") == 0) return TOKEN_KW_WHILE;
            break;
        case 6:
            if (strcmp(text, "import") == 0) return TOKEN_KW_IMPORT;
            if (strcmp(text, "return") == 0) return TOKEN_KW_RETURN;
            if (strcmp(text, "Double") == 0) return TOKEN_TYPE_DOUBLE;
            if (strcmp(text, "String") == 0) return TOKEN_TYPE_STRING;
            break;
    }

    return TOKEN_ID;
}

/**
 * @brief Consumes a nested block comment after "/*" has already been read.
 *
 * If the block comment contains a newline, verifies that no tokens follow it
 * on the same physical line and pushes the terminating '\n' back to stdin.
 *
 * @return  0 if single-line block comment (acts as whitespace)
 *          1 if multi-line block comment (acts as EOL)
 *         -1 on lexical error (error already set in err)
 */
static int consumeBlockComment(Error *err) {
    int depth = 1;
    int hadNewLine = 0;
    int startLine = lineCounter;
    int c;

    while (depth > 0) {
        c = getchar();
        if (c == EOF) {
            handleError(err, LEX_ERROR, "Unclosed block comment", startLine);
            return -1;
        }
        if (c == '\n') {
            lineCounter++;
            hadNewLine = 1;
        } else if (c == '/') {
            int nextC = getchar();
            if (nextC == '*') {
                depth++;
            } else if (nextC == EOF) {
                handleError(err, LEX_ERROR, "Unclosed block comment", startLine);
                return -1;
            } else {
                ungetc(nextC, stdin);
            }
        } else if (c == '*') {
            int nextC = getchar();
            if (nextC == '/') {
                depth--;
            } else if (nextC == EOF) {
                handleError(err, LEX_ERROR, "Unclosed block comment", startLine);
                return -1;
            } else {
                ungetc(nextC, stdin);
            }
        }
    }

    if (hadNewLine) {
        while ((c = getchar()) != '\n' && c != EOF) {
            if (isHorizSpace(c)) {
                continue;
            }
            if (c == '/') {
                int nextC = getchar();
                if (nextC == '/') {
                    while ((c = getchar()) != '\n' && c != EOF);
                    break;
                }
            }
            handleError(err, LEX_ERROR, "Unexpected token after multiline block comment", lineCounter);
            return -1;
        }
        if (c == '\n') {
            ungetc(c, stdin);
        }
        return 1;
    }

    return 0;
}

/**
 * @brief Handles physical newlines and continuation indentation while parenDepth > 0.
 *
 * @return 1 on success, 0 on error (error already set in err)
 */
static int consumeNewlinesInsideParens(Error *err) {
    int spaces = 0;
    int hadTab = 0;

    while (1) {
        int c = getchar();
        if (c == ' ') {
            spaces++;
        } else if (c == '\t') {
            hadTab = 1;
        } else if (c == '\r') {
            continue;
        } else if (c == '\n') {
            lineCounter++;
            spaces = 0;
            hadTab = 0;
        } else if (c == EOF) {
            return 1;
        } else if (c == '/') {
            int nextC = getchar();
            if (nextC == '/') {
                while ((c = getchar()) != '\n' && c != EOF);
                if (c == '\n') {
                    lineCounter++;
                    spaces = 0;
                    hadTab = 0;
                    continue;
                }
                return 1;
            } else if (nextC == '*') {
                int res = consumeBlockComment(err);
                if (res < 0) return 0;
                if (res == 1) {
                    c = getchar();
                    if (c == '\n') {
                        lineCounter++;
                        spaces = 0;
                        hadTab = 0;
                        continue;
                    }
                    return 1;
                }
                continue;
            } else {
                ungetc(nextC, stdin);
                if (hadTab) {
                    handleError(err, LEX_ERROR, "Tabs are not allowed for indentation", lineCounter);
                    return 0;
                }
                if (spaces <= currentLogicalLineIndent) {
                    handleError(err, LEX_ERROR, "Invalid continuation indentation inside parentheses", lineCounter);
                    return 0;
                }
                ungetc(c, stdin);
                return 1;
            }
        } else {
            if (hadTab) {
                handleError(err, LEX_ERROR, "Tabs are not allowed for indentation", lineCounter);
                return 0;
            }
            if (c == ')') {
                if (spaces < currentLogicalLineIndent) {
                    handleError(err, LEX_ERROR, "Invalid closing parenthesis indentation", lineCounter);
                    return 0;
                }
            } else {
                if (spaces <= currentLogicalLineIndent) {
                    handleError(err, LEX_ERROR, "Invalid continuation indentation inside parentheses", lineCounter);
                    return 0;
                }
            }
            ungetc(c, stdin);
            return 1;
        }
    }
}

//Obtains the next token from stdin using a finite state machine
//Uses a buffer to hold context and a stack for emiting dedent tokens.
Token nextToken(Error *err) {
    Token token;
    memset(&token, 0, sizeof(Token));
    token.lineNumber = lineCounter;
    spaceCounter = 0;
    int hadTabOnLine = 0;
    int prevWasUnderscore = 0;
    LexerState state;
    int c;

    //buffer that holds the indentation values
    StringBuffer buffer;
    buffer.capacity = 0;
    buffer.length = 0;
    buffer.text = NULL;

    //first we need to figure out how to handle indentation
    if (owedDedents > 0) {
        owedDedents--;
        token.type = TOKEN_DEDENT;
        return token;
    }
    if (isStartOfLine == 1) {
        state = LINE_START;
    }
    else {
        state = START;
    }

    
    while (1) {
        c = getchar();
        
        switch (state) {
            case START:
                switch (c) {
                    // Structural
                    case EOF:
                        if(stackTop > 0) {
                            owedDedents = stackTop - 1;
                            stackTop = 0;
                            ungetc(c, stdin); //ungetc the EOF so it gets handled properly
                            token.type = TOKEN_DEDENT;
                            return token;
                        } else {
                            token.type = TOKEN_EOF;
                        }
                        return token;
                    case '\n':
                        if (parenDepth > 0) { //new line inside parens should be treated as whitespace
                            lineCounter++;
                            if (!consumeNewlinesInsideParens(err)) {
                                return token;
                            }
                            token.lineNumber = lineCounter;
                            continue;
                        }
                        else {
                            token.type = TOKEN_EOL;
                            token.lineNumber = lineCounter;
                            lineCounter++;
                            isStartOfLine = 1;
                            return token;
                        }
                    case '@':{  //'@' can only be used with "main"
                        int nextC1 = getchar();
                        int nextC2 = getchar();
                        int nextC3 = getchar();
                        int nextC4 = getchar();
                        
                        if (nextC1 == 'm' && nextC2 == 'a' && nextC3 == 'i' && nextC4 == 'n') {
                            int nextC5 = getchar();
                            if (isAsciiAlnum(nextC5) || nextC5 == '_') {
                                handleError(err, LEX_ERROR, "Expected '@main'", token.lineNumber);
                                return token;
                            }
                            ungetc(nextC5, stdin);
                            token.type = TOKEN_ANNOTATION_MAIN;
                            return token;
                        } else {
                            handleError(err, LEX_ERROR, "Expected '@main'", token.lineNumber);
                            return token;
                        }
                    }

                    //operators
                    //single character operators can be found instantly; for the double character ones we can just look one char ahead 
                    case '+': token.type = TOKEN_PLUS; return token;
                    case '-': token.type = TOKEN_MINUS; return token;
                    case '*': token.type = TOKEN_MUL; return token;
                    case '/': {
                        int nextC = getchar();
                        if (nextC == '/') {
                            while ((c = getchar()) != '\n' && c != EOF);
                            if (c == '\n') {
                                ungetc(c, stdin);
                            }
                            continue;
                        } else if (nextC == '*') {
                            if (consumeBlockComment(err) < 0) {
                                return token;
                            }
                            continue;
                        } else { //if it actually wasn't a comment we have to ungetc the one char we read ahead and say it was a division
                            ungetc(nextC, stdin);
                            token.type = TOKEN_DIV;
                            return token;
                        }
                    }
                    case '=': {
                        int nextC = getchar();
                        if (nextC == '=') {
                            token.type = TOKEN_EQ;
                            return token;
                        } else { //if it actually wasn't an equal we have to ungetc the one char we read ahead and say it was an assign
                            ungetc(nextC, stdin);
                            token.type = TOKEN_ASSIGN;
                            return token;
                        }
                    }
                    case '!': {
                        int nextC = getchar();
                        if (nextC == '=') {
                            token.type = TOKEN_NEQ;
                            return token;
                        } else { //there is only one way a '!' can be used and its "!="
                            handleError(err, LEX_ERROR, "Expected '!='", token.lineNumber);
                            return token;
                        }
                    }
                    case '<': {
                        int nextC = getchar();
                        if (nextC == '=') {
                            token.type = TOKEN_LTE;
                            return token;
                        } else { //if it actually wasn't an LTE we have to ungetc the one char we read ahead and say it was a LT
                            ungetc(nextC, stdin);
                            token.type = TOKEN_LT;
                            return token;
                        }
                    }
                    case '>': {
                        int nextC = getchar();
                        if (nextC == '=') {
                            token.type = TOKEN_GTE;
                            return token;
                        } else { //if it actually wasn't a GTE we have to ungetc the one char we read ahead and say it was a GT
                            ungetc(nextC, stdin);
                            token.type = TOKEN_GT;
                            return token;
                        }
                    }

                    //separators
                    case ':': token.type = TOKEN_COLON; return token;
                    case ',': token.type = TOKEN_COMMA; return token;
                    case ';': token.type = TOKEN_SEMICOLON; return token;

                    case '(': {
                        parenDepth++;
                        token.type = TOKEN_LPAREN; 
                        return token;
                    }
                    case ')': {
                        if (parenDepth > 0) {
                            parenDepth--;
                        }
                        
                        token.type = TOKEN_RPAREN; 
                        return token;
                    }
                    case '.': {
                        int nextC = getchar(); //check if nextC is a digit, if it is it's something called "Leading-dot double"
                        if (isAsciiDigit(nextC)) {
                            if (!appendChar(&buffer, c, err)) return token;
                            if (!appendChar(&buffer, nextC, err)) return token;
                            prevWasUnderscore = 0;
                            state = DOUBLE;
                            continue;
                        } else {
                            ungetc(nextC, stdin);
                            token.type = TOKEN_DOT;
                            return token;
                        }
                    }
                    //strings
                    //we have to look 2 chars ahead in case it's a block comment, in case it wasn't we have to ungetc the 2 chars (watchout for the ungetc order)
                    case '"': {
                        int nextC1 = getchar();
                        if (nextC1 == '"') {
                            int nextC2 = getchar();
                            if (nextC2 == '"') {
                                state = BLOCK_STRING;
                                continue;
                            } else {
                                ungetc(nextC2, stdin);
                                token.type = TOKEN_STRING_LITERAL;
                                token.val.stringValue = strdup("");
                                if (token.val.stringValue == NULL) {
                                    handleError(err, INTERNAL_ERROR, "Memory allocation failed", token.lineNumber);
                                }
                                return token;
                            }
                        } else {
                            ungetc(nextC1, stdin);
                            state = STRING;
                            continue;
                        }
                    }
                    
                    //keywords, identifiers, all that
                    default:
                        if (isAsciiAlpha(c) || c == '_') { //alpha chars go into the buffer
                            if (!appendChar(&buffer, c, err)) return token;
                            state = IDENTIFIER;
                            continue;
                        } 
                        else if (isAsciiDigit(c)) { //digits go into the buffer too
                            if (!appendChar(&buffer, c, err)) return token;
                            prevWasUnderscore = 0;
                            state = INT;
                            continue;
                        }
                        else if (isHorizSpace(c)) { //this is absolutely useless
                            continue;
                        }
                        else { //somebody is trying to be funny
                            handleError(err, LEX_ERROR, "Unknown character", token.lineNumber);
                            return token;
                        }
                }
            case LINE_START:
                if (c == ' ') { //count empty spaces
                    spaceCounter++;
                    continue;
                }
                else if (c == '\t') { //refuse tabs on non-empty lines
                    hadTabOnLine = 1;
                    continue;
                }
                else if (c == '\r') {
                    continue;
                }
                else if (c == '\n') { //new line
                    lineCounter++;
                    token.lineNumber = lineCounter;
                    spaceCounter = 0;
                    hadTabOnLine = 0;
                    continue; 
                }
                else if (c == EOF) { //dont forget to handle indentation on EOF
                    if (stackTop > 0) {
                        owedDedents = stackTop;
                        stackTop = 0;
                        ungetc(c, stdin); //ungetc the EOF so it gets handled properly in START state
                        
                        owedDedents--;
                        token.type = TOKEN_DEDENT;
                        return token;
                    }
                    token.lineNumber = lineCounter; //freshen up in case we skipped lines
                    state = START;
                    continue;
                }
                else {
                    if (c == '/') { //there might be a comment at the start of the line
                        int nextC = getchar();
                        if (nextC == '/') { //it's a line comment!
                            while ((c = getchar()) != '\n' && c != EOF); //just throw everything away until '\n' or EOF
                            if (c == '\n') {
                                lineCounter++;
                                token.lineNumber = lineCounter; //so potential errors show on correct lines
                            } else if (c == EOF) { 
                                ungetc(c, stdin); //ungetc the EOF so it gets handled properly in START state
                            }
                            spaceCounter = 0; //reset this before new line !!!
                            hadTabOnLine = 0;
                            continue;
                        } else if (nextC == '*') { //it's a block comment!
                            int res = consumeBlockComment(err);
                            if (res < 0) {
                                return token;
                            }
                            if (res == 1) {
                                c = getchar();
                                if (c == '\n') {
                                    lineCounter++;
                                    token.lineNumber = lineCounter;
                                    spaceCounter = 0;
                                    hadTabOnLine = 0;
                                    continue;
                                } else {
                                    ungetc(c, stdin);
                                    continue;
                                }
                            }
                            while ((c = getchar()) == ' ' || c == '\t' || c == '\r');
                            if (c == '\n' || c == EOF) {
                                ungetc(c, stdin);
                                continue;
                            }
                            ungetc(c, stdin);
                            continue;
                        } else { // It's a division '/' at the start of the line!
                            ungetc(nextC, stdin);
                        }
                    }

                    if (hadTabOnLine) { //refuse tabs
                        handleError(err, LEX_ERROR, "Tabs are not allowed for indentation", lineCounter);
                        return token;
                    }

                    isStartOfLine = 0; //figured out it's no longer a start of line
                    currentLogicalLineIndent = spaceCounter;
                    ungetc(c, stdin); //we know it's time to count indents now, ungetc the first now-whitespace char we found so it doesn't get lost
                    token.lineNumber = lineCounter; //

                    if (spaceCounter > indentStack[stackTop]) { //if there is more spaces than on the stack then we indent
                        if (stackTop + 1 >= MAX_INDENT_STACK) {
                            handleError(err, INTERNAL_ERROR, "Maximum indentation depth exceeded", token.lineNumber);
                            return token;
                        }
                        stackTop++;
                        indentStack[stackTop] = spaceCounter;
                        token.type = TOKEN_INDENT;
                        return token;
                    }
                    else if (spaceCounter < indentStack[stackTop]) { //if there is less spaces than on the stack we dedent
                        while (stackTop > 0 && spaceCounter < indentStack[stackTop]) {
                            stackTop--;
                            owedDedents++;
                        }

                        if (spaceCounter != indentStack[stackTop]) {
                            handleError(err, LEX_ERROR, "Invalid indentation level", token.lineNumber);
                            return token;
                        }

                        owedDedents--;
                        token.type = TOKEN_DEDENT;
                        return token;
                    }
                    else {
                        token.lineNumber = lineCounter; //freshen up in case we skipped lines
                        state = START;
                        continue;
                    }
                }
            case IDENTIFIER: //we need to find out if we are reading an identifier or a keyword
                if (isAsciiAlnum(c) || c == '_') {
                        if (!appendChar(&buffer, c, err)) return token;
                        continue;
                } else {
                        ungetc(c, stdin);
                        
                        token.type = findKeyword(buffer.text, buffer.length, err);
                        
                        if (err->code != SUCCESS) { //check if we passed findKeyword
                            free(buffer.text);
                            return token;
                        }

                        if(token.type == TOKEN_ID) {
                            token.val.stringValue = strdup(buffer.text);
                            if (token.val.stringValue == NULL) {
                                free(buffer.text);
                                handleError(err, INTERNAL_ERROR, "Memory allocation failed", token.lineNumber);
                                return token;
                            }
                        }
                        
                        free(buffer.text);
                        return token;
                }
            case STRING: //just a string, sometimes there is something special in it
                if (c == '"') {
                    token.type = TOKEN_STRING_LITERAL;
                    token.val.stringValue = buffer.text ? strdup(buffer.text) : strdup("");
                    free(buffer.text);
                    if (token.val.stringValue == NULL) {
                        handleError(err, INTERNAL_ERROR, "Memory allocation failed", token.lineNumber);
                    }
                    return token;
                }
                else if (c == '\\') {
                    state = STRING_ESC;
                    continue;
                }
                else if (c == EOF || c == '\n') {
                    free(buffer.text);
                    handleError(err, LEX_ERROR, "Unclosed string", token.lineNumber);
                    return token;
                }
                else if (c < 32 || c > 126) {
                    free(buffer.text);
                    handleError(err, LEX_ERROR, "Invalid character in string literal", token.lineNumber);
                    return token;
                }
                else {
                    if (!appendChar(&buffer, c, err)) return token;
                    continue;
                }
            case STRING_ESC: //that "something special" in a string
                if (c == 'n') {
                    if (!appendChar(&buffer, '\n', err)) return token;
                }
                else if (c == 't') {
                    if (!appendChar(&buffer, '\t', err)) return token;

                }
                else if (c == 'r') {
                    if (!appendChar(&buffer, '\r', err)) return token;
                }
                else if (c == '"') {
                    if (!appendChar(&buffer, '"', err)) return token;
                }
                else if (c == '\\') {
                    if (!appendChar(&buffer, '\\', err)) return token;
                }
                else {
                    free(buffer.text);
                    handleError(err, LEX_ERROR, "Invalid escape sequence", token.lineNumber);
                    return token;
                }

                state = STRING;
                break;
            case BLOCK_STRING: //stupid long string
                if (c == EOF) {
                    free(buffer.text);
                    handleError(err, LEX_ERROR, "Unclosed block string", token.lineNumber);
                    return token;
                }
                else if (c == '\0' || c > 127) {
                    free(buffer.text);
                    handleError(err, LEX_ERROR, "Invalid character in multiline string literal", token.lineNumber);
                    return token;
                }
                else if(c == '\n') {
                    lineCounter++;
                    if (!appendChar(&buffer, c, err)) return token;
                    continue;
                }
                else if (c == '"') { //always look 2 chars ahead until you find """; watchout to ungetc in correct order
                    int nextC1 = getchar();
                    if (nextC1 == '"') {
                        int nextC2 = getchar();
                        if (nextC2 == '"') {
                            int extra;
                            while ((extra = getchar()) == '"') {
                                if (!appendChar(&buffer, '"', err)) return token;
                            }
                            ungetc(extra, stdin);
                            token.type = TOKEN_STRING_LITERAL;
                            token.val.stringValue = buffer.text ? strdup(buffer.text) : strdup("");
                            free(buffer.text);
                            if (token.val.stringValue == NULL) {
                                handleError(err, INTERNAL_ERROR, "Memory allocation failed", token.lineNumber);
                            }
                            return token;
                        }
                        ungetc(nextC2, stdin);
                        if (!appendChar(&buffer, '"', err)) return token;
                        if (!appendChar(&buffer, '"', err)) return token;
                        continue;
                    } else {
                        ungetc(nextC1, stdin);
                        if (!appendChar(&buffer, '"', err)) return token;
                        continue;
                    }
                }
                else {
                    if (!appendChar(&buffer, c, err)) return token;
                    continue; 
                }
            case INT: 
                if (isAsciiDigit(c)) { //check if we are not reading a "Leading-0 INT"; if not then add the digit
                    if (buffer.length == 1 && buffer.text[0] == '0') {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Leading zeros are not allowed in integer literals", token.lineNumber);
                        return token;
                    }
                    prevWasUnderscore = 0;
                    if (!appendChar(&buffer, c, err)) return token;
                    continue;
                }
                else if (c == '_') { //check if we are not reading a "Leading-0 INT"; if not then just skip the '_' and continue
                    if (prevWasUnderscore || (buffer.length == 1 && buffer.text[0] == '0')) {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Leading zeros are not allowed in integer literals", token.lineNumber);
                        return token;
                    }
                    prevWasUnderscore = 1;
                    //do NOT append, just get rid of it
                    continue;
                }
                else if (c == '.') { //check if the number continues after the dot, then go to double
                    if (prevWasUnderscore) {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Underscore cannot precede decimal point", token.lineNumber);
                        return token;
                    }
                    int nextC = getchar();
                    if (!isAsciiDigit(nextC)) {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Digit missing after '.'", token.lineNumber);
                        return token;
                    }
                    if (!appendChar(&buffer, c, err)) return token;
                    if (!appendChar(&buffer, nextC, err)) return token;
                    prevWasUnderscore = 0;
                    state = DOUBLE;
                    continue;
                }
                else if (c == 'e' || c == 'E') { //watchout for the optional '+' or '-'; if there is no digit after then throw error, otherwise go to the weird double
                    if (prevWasUnderscore) {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Underscore cannot precede exponent", token.lineNumber);
                        return token;
                    }
                    if (!appendChar(&buffer, c, err)) return token;
                    int nextC = getchar();
                    if (nextC == '+' || nextC == '-') {
                        if (!appendChar(&buffer, nextC, err)) return token;
                        nextC = getchar();
                    }
                    if (!isAsciiDigit(nextC)) {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Invalid exponent in double literal", token.lineNumber);
                        return token;
                    }
                    if (!appendChar(&buffer, nextC, err)) return token;
                    prevWasUnderscore = 0;
                    state = DOUBLE_EXP;
                    continue;
                }
                else { //the int ended, ungetc whatever ended it and return the token
                    if (prevWasUnderscore || isAsciiAlpha(c) || c == '_') {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Invalid character in integer literal", token.lineNumber);
                        return token;
                    }
                    ungetc(c, stdin);
                    token.type = TOKEN_INT_LITERAL;
                    token.val.intValue = (int)strtoll(buffer.text, NULL, 10);
                    free(buffer.text);
                    return token;
                }
            case DOUBLE:
                if (isAsciiDigit(c)) { //add a digit
                    prevWasUnderscore = 0;
                    if (!appendChar(&buffer, c, err)) return token;
                    continue;
                }
                else if (c == '_') {
                    if (prevWasUnderscore) {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Consecutive underscores in double literal", token.lineNumber);
                        return token;
                    }
                    prevWasUnderscore = 1;
                    //do NOT append, just get rid of it
                    continue;
                }
                else if (c == 'e' || c == 'E') { //watchout for the optional '+' or '-'; if there is no digit after then throw error, otherwise go to the weird double
                    if (prevWasUnderscore) {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Underscore cannot precede exponent", token.lineNumber);
                        return token;
                    }
                    if (!appendChar(&buffer, c, err)) return token;
                    int nextC = getchar();
                    if (nextC == '+' || nextC == '-') {
                        if (!appendChar(&buffer, nextC, err)) return token;
                        nextC = getchar();
                    }
                    if (!isAsciiDigit(nextC)) {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Invalid exponent in double literal", token.lineNumber);
                        return token;
                    }
                    if (!appendChar(&buffer, nextC, err)) return token;
                    prevWasUnderscore = 0;
                    state = DOUBLE_EXP;
                    continue;
                }
                else if (c == 'd' || c == 'D') {
                    if (prevWasUnderscore) {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Underscore cannot precede 'd' suffix", token.lineNumber);
                        return token;
                    }
                    int nextC = getchar();
                    if (isAsciiAlnum(nextC) || nextC == '_' || nextC == '.') {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Invalid character after double literal", token.lineNumber);
                        return token;
                    }
                    ungetc(nextC, stdin);
                    //do NOT append, just get rid of it, it's optional; also means the double ended, return the token (do not ungetc here, we dont care about the 'd' / 'D')
                    token.type = TOKEN_DOUBLE_LITERAL;
                    token.val.doubleValue = strtod(buffer.text, NULL);
                    free(buffer.text);
                    return token;
                }
                else { //the double ended, ungetc whatever ended it and return the token
                    if (prevWasUnderscore || isAsciiAlpha(c) || c == '_' || c == '.') {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Invalid character in double literal", token.lineNumber);
                        return token;
                    }
                    ungetc(c, stdin);
                    token.type = TOKEN_DOUBLE_LITERAL;
                    token.val.doubleValue = strtod(buffer.text, NULL);
                    free(buffer.text);
                    return token;
                }
            case DOUBLE_EXP: //(this is the weird double)
                if (isAsciiDigit(c)) { //add a digit
                    prevWasUnderscore = 0;
                    if (!appendChar(&buffer, c, err)) return token;
                    continue;
                }
                else if (c == '_') {
                    if (prevWasUnderscore) {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Consecutive underscores in double exponent", token.lineNumber);
                        return token;
                    }
                    prevWasUnderscore = 1;
                    //do NOT append, just get rid of it
                    continue;
                }
                else if (c == 'd' || c == 'D') {
                    if (prevWasUnderscore) {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Underscore cannot precede 'd' suffix", token.lineNumber);
                        return token;
                    }
                    int nextC = getchar();
                    if (isAsciiAlnum(nextC) || nextC == '_' || nextC == '.') {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Invalid character after double literal", token.lineNumber);
                        return token;
                    }
                    ungetc(nextC, stdin);
                    //do NOT append, just get rid of it, it's optional; also means the double ended, return the token (do not ungetc here, we dont care about the 'd' / 'D')
                    token.type = TOKEN_DOUBLE_LITERAL;
                    token.val.doubleValue = strtod(buffer.text, NULL);
                    free(buffer.text);
                    return token;
                }
                else { //the weird double ended, ungetc whatever ended it and return the token
                    if (prevWasUnderscore || isAsciiAlpha(c) || c == '_' || c == '.') {
                        free(buffer.text);
                        handleError(err, LEX_ERROR, "Invalid character in double exponent", token.lineNumber);
                        return token;
                    }
                    ungetc(c, stdin);
                    token.type = TOKEN_DOUBLE_LITERAL;
                    token.val.doubleValue = strtod(buffer.text, NULL);
                    free(buffer.text);
                    return token;
                }
            default: //keep this here so gcc -Wall -Wextra doesn't complain
                //if we get here something is VERY WRONG
                break;
        }
    }
} //the end - yaaaay :D