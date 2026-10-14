#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "init.h"

typedef enum {
    START,

    LINE_START,

    IDENTIFIER,

    INT,
    DOUBLE,
    DOUBLE_EXP,
    
    STRING,
    STRING_ESC,
    BLOCK_STRING,

    MORE,
    LESS,
    SLASH,

    LINE_COMMENT,
    BLOCK_COMMENT,

} LexerState;

typedef struct {
    char *text;
    int length;
    int capacity;
} StringBuffer;

static int lineCounter = 1;
static int spaceCounter = 0;
static int indentStack[100] = {0};
static int stackTop = 0;
static int owedDedents = 0;
static int isStartOfLine = 0;

static void appendChar(StringBuffer *buffer, int c) {
    if (buffer->length + 1 >= buffer->capacity) {
        buffer->capacity *= 2;
        buffer->text = realloc(buffer->text, buffer->capacity);
        if (buffer->text == NULL) {
            exit(99);
        }
    }
    buffer->text[buffer->length++] = (char) c;
    buffer->text[buffer->length] = '\0';
}

static TokenType findKeyword(const char *text, const int len, const int lineNumber /*purely for error msgs*/) {
    switch (len) {
        case 1:
            if (text[0] == '_') {
                fprintf(stderr, "Lexical error on line %d, '_' itself cannot be an identifier\n", lineNumber);
                exit(1);
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

Token nextToken() {
    Token token;
    token.lineNumber = lineCounter;
    spaceCounter = 0;
    LexerState state;
    int c;

    StringBuffer buffer;
    buffer.capacity = 256;
    buffer.length = 0;
    buffer.text = malloc(buffer.capacity);

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
                        token.type = TOKEN_EOF;
                        return token;
                    case '\n':
                        token.type = TOKEN_EOL;
                        token.lineNumber = lineCounter;
                        lineCounter++;
                        isStartOfLine = 1;
                        return token;
                    case '@':{  //'@' can only be used with "main"
                        int nextC1 = getchar();
                        int nextC2 = getchar();
                        int nextC3 = getchar();
                        int nextC4 = getchar();
                        
                        if (nextC1 == 'm' && nextC2 == 'a' && nextC3 == 'i' && nextC4 == 'n') {
                            token.type = TOKEN_ANNOTATION_MAIN;
                            return token;
                        } else {
                            fprintf(stderr, "Lexical error on line %d: Expected '@main'\n", token.lineNumber);
                            exit(1);
                        }
                    }

                    //operators
                    case '+': token.type = TOKEN_PLUS; return token;
                    case '-': token.type = TOKEN_MINUS; return token;
                    case '*': token.type = TOKEN_MUL; return token;
                    case '/': {
                        int nextC = getchar();
                        if (nextC == '/') {
                            state = LINE_COMMENT;
                            continue;
                        } else if (nextC == '*') {
                            state = BLOCK_COMMENT;
                            continue;
                        } else {
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
                        } else {
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
                        } else {
                            fprintf(stderr, "Lexical error on line %d: Expected '!='\n", token.lineNumber);
                            exit(1);
                        }
                    }
                    case '<': {
                        int nextC = getchar();
                        if (nextC == '=') {
                            token.type = TOKEN_LTE;
                            return token;
                        } else {
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
                        } else {
                            ungetc(nextC, stdin);
                            token.type = TOKEN_GT;
                            return token;
                        }
                    }

                    //separators
                    case '(': token.type = TOKEN_LPAREN; return token;
                    case ')': token.type = TOKEN_RPAREN; return token;
                    case ':': token.type = TOKEN_COLON; return token;
                    case ',': token.type = TOKEN_COMMA; return token;
                    case '.': token.type = TOKEN_DOT; return token;
                    case ';': token.type = TOKEN_SEMICOLON; return token;
                    
                    //strings
                    case '"': {
                        int nextC1 = getchar();
                        int nextC2 = getchar();
                        if (nextC1 == '"' && nextC2 == '"') {
                            state = BLOCK_STRING;
                            continue;
                        } else {
                            ungetc(nextC2, stdin);
                            ungetc(nextC1, stdin);
                            state = STRING;
                            continue;
                        }
                    }
                    
                    //keywords, identifiers, all that
                    default:
                        if (isalpha(c) || c == '_') {
                            appendChar(&buffer, c);
                            state = IDENTIFIER;
                            continue;
                        } 
                        else if (isdigit(c)) {
                            appendChar(&buffer, c);
                            state = INT;
                            continue;
                        }
                        else if (isspace(c)) {
                            //just throw this away
                            continue;
                        }
                        else {
                            fprintf(stderr, "Lexical error on line %d: Unknown character '%c'\n", token.lineNumber, c);
                            exit(1);
                        }
                }
                break;
            case LINE_START:
                if (c == ' ') {
                    spaceCounter++;
                    continue;
                }
                else if (c == '\t') {
                    fprintf(stderr, "Lexical error on line %d: Tabs are not allowed for indentation\n", token.lineNumber);
                    exit(1);
                }
                else if (c == '\n') {
                    lineCounter++;
                    spaceCounter = 0;
                    continue; 
                }
                else if (c == EOF) {
                    if (stackTop > 0) {
                        owedDedents = stackTop;
                        stackTop = 0;
                        ungetc(c, stdin); 
                        
                        owedDedents--;
                        token.type = TOKEN_DEDENT;
                        return token;
                    }
                    state = START;
                    continue;
                }
                else {
                    isStartOfLine = 0;
                    ungetc(c, stdin);

                    if (spaceCounter > indentStack[stackTop]) {
                        stackTop++;
                        indentStack[stackTop] = spaceCounter;
                        token.type = TOKEN_INDENT;
                        return token;
                    }
                    else if (spaceCounter < indentStack[stackTop]) {
                        while (stackTop > 0 && spaceCounter < indentStack[stackTop]) {
                            stackTop--;
                            owedDedents++;
                        }

                        if (spaceCounter != indentStack[stackTop]) {
                            fprintf(stderr, "Lexical error on line %d: Invalid dedent level\n", token.lineNumber);
                            exit(1);
                        }

                        owedDedents--;
                        token.type = TOKEN_DEDENT;
                        return token;
                    }
                    else {
                        state = START;
                        continue;
                    }
                }
                break;
            case IDENTIFIER:
                if (isalnum(c) || c == '_') {
                        appendChar(&buffer, c);
                        continue;
                    } else {
                        ungetc(c, stdin);
                        
                        token.type = findKeyword(buffer.text, buffer.length, token.lineNumber);
                        
                        if(token.type == TOKEN_ID) {
                            token.stringValue = strdup(buffer.text);
                        }
                        
                        free(buffer.text);
                        return token;
                    }
                break;
            case STRING:
                if (c == '"') {
                    token.type = TOKEN_STRING_LITERAL;
                    token.stringValue = strdup(buffer.text);
                    free(buffer.text);
                    return token;
                }
                else if (c == '\\') {
                    state = STRING_ESC;
                    continue;
                }
                else if (c == EOF || c == '\n') {
                    fprintf(stderr, "Lexical error on line %d: unclosed string\n", token.lineNumber);
                    exit(1);
                }
                else {
                    appendChar(&buffer, c);
                    continue;
                }
                break;
            case STRING_ESC:
                if (c == 'n') {
                    appendChar(&buffer, '\n');
                }
                else if (c == 't') {
                    appendChar(&buffer, '\t');
                }
                else if (c == 'r') {
                    appendChar(&buffer, '\r');
                }
                else if (c == '"') {
                    appendChar(&buffer, '"');
                }
                else if (c == '\\') {
                    appendChar(&buffer, '\\');
                }
                else {
                    fprintf(stderr, "Lexical error on line %d: invalid escape sequence \\%c\n", token.lineNumber, c);
                }

                state = STRING;
                break;
            case BLOCK_STRING:
                if (c == EOF) {
                    fprintf(stderr, "Lexical error on line %d: unclosed block string\n");
                    exit(1);
                }
                else if(c == '\n') {
                    lineCounter++;
                    appendChar(&buffer, c);
                    continue;
                }
                else if (c == '"') {
                    int nextC1 = getchar();
                    int nextC2 = getchar();
                    if (nextC1 == '"' && nextC2 == '"') {
                        token.type = TOKEN_STRING_LITERAL;
                        token.stringValue = strdup(buffer.text);
                        free(buffer.text);
                        return token;
                    }
                    else {
                        ungetc(nextC2, stdin);
                        ungetc(nextC1, stdin);
                        appendChar(&buffer, c);
                        continue;
                    }
                }
                else {
                    appendChar(&buffer, c);
                    continue; 
                }
                break;
            case INT:
                if (isdigit(c)) {
                    appendChar(&buffer, c);
                    continue;
                }
                else if (c == '_') {
                    //do NOT append, just get rid of it
                    continue;
                }
                else if (c == '.') {
                    appendChar(&buffer, c);
                    state = DOUBLE;
                    continue;
                }
                else if (c == 'e' || c == 'E') {
                    appendChar(&buffer, c);
                    state = DOUBLE_EXP;
                    continue;
                }
                else {
                    ungetc(c, stdin);
                    token.type = TOKEN_INT_LITERAL;
                    token.intValue = (int)strtol(buffer.text, NULL, 10);
                    free(buffer.text);
                    return token;
                }
                break;
            case DOUBLE:
                if (isdigit(c)) {
                    appendChar(&buffer, c);
                    continue;
                }
                else if (c == '_') {
                    //do NOT append, just get rid of it
                    continue;
                }
                else if (c == 'e' || c == 'E') {
                    appendChar(&buffer, c);
                    state = DOUBLE_EXP;
                    continue;
                }
                else if (c == 'd' || c == 'D') {
                    //do NOT append, just get rid of it, it's optional
                    token.type = TOKEN_DOUBLE_LITERAL;
                    token.doubleValue = strtod(buffer.text, NULL);
                    free(buffer.text);
                    return token;
                }
                else {
                    ungetc(c, stdin);
                    token.type = TOKEN_DOUBLE_LITERAL;
                    token.doubleValue = strtod(buffer.text, NULL);
                    free(buffer.text);
                    return token;
                break;
            case DOUBLE_EXP:
                if (isdigit(c)) {
                    appendChar(&buffer, c);
                    continue;
                }
                else if (c == '_') {
                    //do NOT append, just get rid of it
                    continue;
                }
                else if (c == 'd' || c == 'D') {
                    //do NOT append, just get rid of it, it's optional
                    token.type = TOKEN_DOUBLE_LITERAL;
                    token.doubleValue = strtod(buffer.text, NULL);
                    free(buffer.text);
                    return token;
                }
                else {
                    ungetc(c, stdin);
                    token.type = TOKEN_DOUBLE_LITERAL;
                    token.doubleValue = strtod(buffer.text, NULL);
                    free(buffer.text);
                    return token;
                }
                break;
            case LINE_COMMENT:
                if (c == '\n') {
                    ungetc(c, stdin);
                    state = START;
                }
                else if (c == EOF) {
                    ungetc(c, stdin);
                    state = START;
                }
                break;
            case BLOCK_COMMENT:
                if (c == '*') {
                    int nextC = getchar();
                    if (nextC == '/') {
                        state = START;
                    }
                    else { 
                        ungetc(nextC, stdin);
                    }
                }
                else if (c == '\n') {
                    lineCounter++;
                }
                else if (c == EOF) {
                    fprintf(stderr, "Lexical error: Unclosed block comment starting at line %d\n", token.lineNumber);
                    exit(1); 
                }
                break;
            default:
                
                break;
            }
        }
    }
}
