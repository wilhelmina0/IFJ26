/**
 * @file init.h
 * @brief Public interface for the lexical analyzer
 * 
 * Exposes the nextToken() function prototype for the syntax analyzer.
 * 
 * @authors
 * - Tadeáš Vykopal (xvykopt00)
 * 
 * @date 2026
 */

#ifndef LEX_INIT_H
#define LEX_INIT_H
#include "../core/error.h"
#include "../core/token.h"

/**
* @brief Obtains the next token from stdin using a finite state machine
*
* Uses a buffer to hold context and a line for emiting dedent tokens.
*
* @return The struct Token obtaining the token type and in some cases it's value in a union
*
* @warning The caller must check error code after every call!!! If there is an error the token can and will contain garbage values!
*/
Token nextToken(Error *err);

typedef enum {
    //structural
    TOKEN_EOF,
    TOKEN_EOL,
    TOKEN_ANNOTATION_MAIN, // @main // "@" can ONLY be used with the KW main, so we dont need a separate "@" token

    //keywords
    TOKEN_KW_DEF,
    TOKEN_KW_DO,
    TOKEN_KW_ELSE,
    TOKEN_KW_IF,
    TOKEN_KW_IMPORT,
    TOKEN_KW_RETURN,
    TOKEN_KW_THEN,
    TOKEN_KW_VAL,
    TOKEN_KW_VAR,
    TOKEN_KW_WHILE,
    
    //identifiers and literals
    TOKEN_ID,
    TOKEN_INT_LITERAL,
    TOKEN_DOUBLE_LITERAL,
    TOKEN_STRING_LITERAL,

    //types
    TOKEN_TYPE_INT,
    TOKEN_TYPE_DOUBLE,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_UNIT,

    //operators
    TOKEN_ASSIGN,    // =
    TOKEN_PLUS,      // +
    TOKEN_MINUS,     // -
    TOKEN_MUL,       // *
    TOKEN_DIV,       // /
    TOKEN_EQ,        // ==
    TOKEN_NEQ,       // !=
    TOKEN_LT,        // <
    TOKEN_LTE,       // <=
    TOKEN_GT,        // >
    TOKEN_GTE,       // >=

    //separators
    TOKEN_LPAREN,    // (
    TOKEN_RPAREN,    // )
    TOKEN_COLON,     // :
    TOKEN_COMMA,     // ,
    TOKEN_DOT,       // .
    TOKEN_SEMICOLON, // ;
} TokenType;

typedef struct TOKEN
{
    TokenType type;
    union { //found out the previously suggested void pointer method is about a bazoogalion times slower and harder to code than this due to constant mallocs, frees and shit like that
        int intValue;
        double doubleValue;
        char *stringValue;
    };

    int lineNumber;
    //int col_number; //we could have prettier error msgs with this but it would slow us down
} Token;

Token nextToken();

#endif //LEX_INIT_H
