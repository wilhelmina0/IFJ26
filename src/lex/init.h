
#ifndef LEX_INIT_H
#define LEX_INIT_H

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
