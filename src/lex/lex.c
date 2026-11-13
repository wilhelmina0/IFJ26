/**
* @file lex.c
* @brief Implementation of lexical analyzer for the language IFJ26
*
* This module reads input from stdin, manages indentation and generates a stream of tokens for the syntax analyzer.
*
* @author Tadeáš Vykopal (xvykopt00)
* @date 2026
*/

#define _POSIX_C_SOURCE 200809L //so the strdup works if we use C99
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "init.h"

static int lineCounter = 1;
static int spaceCounter = 0;
static int indentStack[100] = {0};
static int stackTop = 0;
static int owedDedents = 0;
static int isStartOfLine = 1;

/**
* @brief Adds a char to the dynamic buffer and allocates more memory if it's needed.
* 
* @param buffer Pointer to the initialized struct StringBuffer
* @param c The char to add (obtained using getchar())
*/
static void appendChar(StringBuffer *buffer, int c) {
    if (buffer->text == NULL) {
        buffer->capacity = 64;
        buffer->text = malloc(buffer->capacity);
        if (buffer->text == NULL) exit(99);
    } else if (buffer->length + 1 >= buffer->capacity) {
        buffer->capacity *= 2;
        buffer->text = realloc(buffer->text, buffer->capacity);
        if (buffer->text == NULL) exit(99);
    }
    buffer->text[buffer->length++] = (char) c;
    buffer->text[buffer->length] = '\0';
}

/**
* @brief Finds out if the loaded string is a keyword or an identifier.
*
* Utilizes string length to make the lookup faster
*
* @param text Pointer to the loaded string
* @param len Length of the string
* @param lineNumber The current line (purely for error msgs)
* 
* @return Token type
*/
static TokenType findKeyword(const char *text, const int len, const int lineNumber) {
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


//Obtains the next token from stdin using a finite state machine
//Uses a buffer to hold context and a stack for emiting dedent tokens.
Token nextToken() {
    Token token;
    token.lineNumber = lineCounter;
    spaceCounter = 0;
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
                    //single character operators can be found instantly; for the double character ones we can just look one char ahead 
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
                            fprintf(stderr, "Lexical error on line %d: Expected '!='\n", token.lineNumber);
                            exit(1);
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
                    //really simple, all of them are single char
                    case '(': token.type = TOKEN_LPAREN; return token;
                    case ')': token.type = TOKEN_RPAREN; return token;
                    case ':': token.type = TOKEN_COLON; return token;
                    case ',': token.type = TOKEN_COMMA; return token;
                    case '.': token.type = TOKEN_DOT; return token;
                    case ';': token.type = TOKEN_SEMICOLON; return token;
                    
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
                        if (isalpha(c) || c == '_') { //alpha chars go into the buffer
                            appendChar(&buffer, c);
                            state = IDENTIFIER;
                            continue;
                        } 
                        else if (isdigit(c)) { //digits go into the buffer too
                            appendChar(&buffer, c);
                            state = INT;
                            continue;
                        }
                        else if (isspace(c)) { //this is absolutely useless
                            continue;
                        }
                        else { //somebody is trying to be funny
                            fprintf(stderr, "Lexical error on line %d: Unknown character '%c'\n", token.lineNumber, c);
                            exit(1);
                        }
                }
                break;
            case LINE_START:
                if (c == ' ') { //count empty spaces
                    spaceCounter++;
                    continue;
                }
                else if (c == '\t') { //refuse tabs
                    fprintf(stderr, "Lexical error on line %d: Tabs are not allowed for indentation\n", token.lineNumber);
                    exit(1);
                }
                else if (c == '\n') { //new line
                    lineCounter++;
                    spaceCounter = 0;
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
                            } else if (c == EOF) { 
                                ungetc(c, stdin); //ungetc the EOF so it gets handled properly in START state
                            }
                            spaceCounter = 0; //reset this before new line !!!
                            continue;
                        } else if (nextC == '*') { //it's a block comment!
                            int prev = 0; //we have to look back at the previous char instead of the next one because comments are just weird...
                            while ((c = getchar()) != EOF) {
                                if (c == '\n') {
                                    lineCounter++;
                                    spaceCounter = 0; // Reset spaces if comment spans multiple lines
                                }
                                if (prev == '*' && c == '/') { //look back if we ended the comment or there just was a * in the comment
                                    break;
                                }
                                prev = c;
                            }
                            if (c == EOF) { //we do NOT ungetc the EOF here because the block comments need to be closed before EOF
                                fprintf(stderr, "Lexical error: Unclosed block comment starting at line %d\n", token.lineNumber);
                                exit(1);
                            }
                            continue;
                        } else { // It's a division '/' at the start of the line!
                            ungetc(nextC, stdin);
                        }
                    }

                    isStartOfLine = 0; //figured out it's no longer a start of line
                    ungetc(c, stdin); //we know it's time to count indents now, ungetc the first now-whitespace char we found so it doesn't get lost

                    if (spaceCounter > indentStack[stackTop]) { //if there is more spaces than on the stack then we indent
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
            case IDENTIFIER: //we need to find out if we are reading an identifier or a keyword
                if (isalnum(c) || c == '_') {
                        appendChar(&buffer, c);
                        continue;
                } else {
                        ungetc(c, stdin);
                        
                        token.type = findKeyword(buffer.text, buffer.length, token.lineNumber);
                        
                        if(token.type == TOKEN_ID) {
                            token.val.stringValue = strdup(buffer.text);
                        }
                        
                        free(buffer.text);
                        return token;
                }
            case STRING: //just a string, sometimes there is something special in it
                if (c == '"') {
                    token.type = TOKEN_STRING_LITERAL;
                    token.val.stringValue = buffer.text ? strdup(buffer.text) : strdup("");
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
            case STRING_ESC: //that "something special" in a string
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
                    exit(1);
                }

                state = STRING;
                break;
            case BLOCK_STRING: //stupid long string
                if (c == EOF) {
                    fprintf(stderr, "Lexical error on line %d: unclosed block string\n", token.lineNumber);
                    exit(1);
                }
                else if(c == '\n') {
                    lineCounter++;
                    appendChar(&buffer, c);
                    continue;
                }
                else if (c == '"') { //always look 2 chars ahead until you find """; watchout to ungetc in correct order
                    int nextC1 = getchar();
                    if (nextC1 == '"') {
                        int nextC2 = getchar();
                        if (nextC2 == '"') {
                            token.type = TOKEN_STRING_LITERAL;
                            token.val.stringValue = buffer.text ? strdup(buffer.text) : strdup("");
                            free(buffer.text);
                            return token;
                        }
                        ungetc(nextC2, stdin);
                        appendChar(&buffer, '"');
                        appendChar(&buffer, '"');
                        continue;
                    } else {
                        ungetc(nextC1, stdin);
                        appendChar(&buffer, '"');
                        continue;
                    }
                }
                else {
                    appendChar(&buffer, c);
                    continue; 
                }
                break;
            case INT: 
                if (isdigit(c)) { //add a digit
                    appendChar(&buffer, c);
                    continue;
                }
                else if (c == '_') {
                    //do NOT append, just get rid of it
                    continue;
                }
                else if (c == '.') { //go to double
                    appendChar(&buffer, c);
                    state = DOUBLE;
                    continue;
                }
                else if (c == 'e' || c == 'E') { //go to the weird double
                    appendChar(&buffer, c);
                    state = DOUBLE_EXP;
                    continue;
                }
                else { //the int ended, ungetc whatever ended it and return the token
                    ungetc(c, stdin);
                    token.type = TOKEN_INT_LITERAL;
                    token.val.intValue = (int)strtol(buffer.text, NULL, 10);
                    free(buffer.text);
                    return token;
                }
            case DOUBLE:
                if (isdigit(c)) { //add a digit
                    appendChar(&buffer, c);
                    continue;
                }
                else if (c == '_') {
                    //do NOT append, just get rid of it
                    continue;
                }
                else if (c == 'e' || c == 'E') { //go to the weird double
                    appendChar(&buffer, c);
                    state = DOUBLE_EXP;
                    continue;
                }
                else if (c == 'd' || c == 'D') {
                    //do NOT append, just get rid of it, it's optional; also means the double ended, return the token (do not ungetc here, we dont care about the 'd' / 'D')
                    token.type = TOKEN_DOUBLE_LITERAL;
                    token.val.doubleValue = strtod(buffer.text, NULL);
                    free(buffer.text);
                    return token;
                }
                else { //the double ended, ungetc whatever ended it and return the token
                    ungetc(c, stdin);
                    token.type = TOKEN_DOUBLE_LITERAL;
                    token.val.doubleValue = strtod(buffer.text, NULL);
                    free(buffer.text);
                    return token;
                }
            case DOUBLE_EXP: //(this is the weird double)
                if (isdigit(c)) { //add a digit
                    appendChar(&buffer, c);
                    continue;
                }
                else if (c == '_') {
                    //do NOT append, just get rid of it
                    continue;
                }
                else if (c == 'd' || c == 'D') {
                    //do NOT append, just get rid of it, it's optional; also means the double ended, return the token (do not ungetc here, we dont care about the 'd' / 'D')
                    token.type = TOKEN_DOUBLE_LITERAL;
                    token.val.doubleValue = strtod(buffer.text, NULL);
                    free(buffer.text);
                    return token;
                }
                else { //the weird double ended, ungetc whatever ended it and return the token
                    ungetc(c, stdin);
                    token.type = TOKEN_DOUBLE_LITERAL;
                    token.val.doubleValue = strtod(buffer.text, NULL);
                    free(buffer.text);
                    return token;
                }
            case LINE_COMMENT: //just look for '\n' or EOF
                if (c == '\n') {
                    ungetc(c, stdin);
                    state = START;
                }
                else if (c == EOF) {
                    ungetc(c, stdin);
                    state = START;
                }
                break;
            case BLOCK_COMMENT: //just look for '*/' or EOF
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
        }
    }
} //the end - yaaaay :D
