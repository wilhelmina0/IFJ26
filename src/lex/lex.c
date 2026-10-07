#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "init.h"

typedef enum {
    START,
    IDENTIFIER
} LexerState;

typedef struct {
    char *text;
    int length;
    int capacity;
} StringBuffer;

static int lineCounter = 1;

void appendChar(StringBuffer *buffer, int c) {
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

TokenType findKeyword(const char *text, int lineNumber) { 
    if (strcmp(text, "def") == 0) return TOKEN_KW_DEF;
    if (strcmp(text, "do") == 0) return TOKEN_KW_DO;
    if (strcmp(text, "else") == 0) return TOKEN_KW_ELSE;
    if (strcmp(text, "if") == 0) return TOKEN_KW_IF;
    if (strcmp(text, "import") == 0) return TOKEN_KW_IMPORT;
    if (strcmp(text, "return") == 0) return TOKEN_KW_RETURN;
    if (strcmp(text, "then") == 0) return TOKEN_KW_THEN;
    if (strcmp(text, "val") == 0) return TOKEN_KW_VAL;
    if (strcmp(text, "var") == 0) return TOKEN_KW_VAR;
    if (strcmp(text, "while") == 0) return TOKEN_KW_WHILE;
    
    if (strcmp(text, "Int") == 0) return TOKEN_TYPE_INT;
    if (strcmp(text, "Double") == 0) return TOKEN_TYPE_DOUBLE;
    if (strcmp(text, "String") == 0) return TOKEN_TYPE_STRING;
    if (strcmp(text, "Unit") == 0) return TOKEN_TYPE_UNIT;

    if(strcmp(text, "_") == 0) {
        fprintf(stderr, "Lexical error on line %d, '_' itself cannot be an identifier\n", lineNumber);
        exit(1);
    }

    return TOKEN_ID;
}

Token nextToken() {
    Token token;
    token.lineNumber = lineCounter;
    LexerState state = START;
    int c;

    StringBuffer buffer;
    buffer.capacity = 256; 
    buffer.length = 0;
    buffer.text = malloc(buffer.capacity);
    
    while (1) {
        c = getchar();
        
        switch (state) {
            case START: 
                if (c == EOF) {
                    token.type = TOKEN_EOF;
                    return token;
                }
                else if (c == '\n') {
                    token.type = TOKEN_EOL;
                    token.lineNumber = lineCounter;
                    lineCounter++;
                    return token;
                }
                else if (c == '@') { 
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
                else if (isalpha(c) || c == '_') {
                    appendChar(&buffer, c);
                    state = IDENTIFIER;
                    continue;
                }
                else if (isspace(c)) {
                    continue;
                }
                break;

            case IDENTIFIER:
                if (isalnum(c) || c == '_') {
                    appendChar(&buffer, c);
                    continue;
                } else {
                    ungetc(c, stdin);
                    
                    token.type = findKeyword(buffer.text, token.lineNumber);
                    
                    if(token.type == TOKEN_ID) {
                        token.stringValue = strdup(buffer.text);
                    }
                    
                    free(buffer.text);
                    return token;
                }
                break;

            default:
                break;
        }
    }
}