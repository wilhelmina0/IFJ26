#include <stdio.h>
#include <stdlib.h>
#include "../../src/lex/init.h"

static const char *tokenName(TokenType type) {
    switch (type) {
        case TOKEN_EOF: return "EOF";
        case TOKEN_EOL: return "EOL";
        case TOKEN_ANNOTATION_MAIN: return "@main";
        case TOKEN_INDENT: return "INDENT";
        case TOKEN_DEDENT: return "DEDENT";
        case TOKEN_KW_DEF: return "KW_DEF";
        case TOKEN_KW_DO: return "KW_DO";
        case TOKEN_KW_ELSE: return "KW_ELSE";
        case TOKEN_KW_IF: return "KW_IF";
        case TOKEN_KW_IMPORT: return "KW_IMPORT";
        case TOKEN_KW_RETURN: return "KW_RETURN";
        case TOKEN_KW_THEN: return "KW_THEN";
        case TOKEN_KW_VAL: return "KW_VAL";
        case TOKEN_KW_VAR: return "KW_VAR";
        case TOKEN_KW_WHILE: return "KW_WHILE";
        case TOKEN_ID: return "ID";
        case TOKEN_INT_LITERAL: return "INT";
        case TOKEN_DOUBLE_LITERAL: return "DOUBLE";
        case TOKEN_STRING_LITERAL: return "STRING";
        case TOKEN_TYPE_INT: return "TYPE_INT";
        case TOKEN_TYPE_DOUBLE: return "TYPE_DOUBLE";
        case TOKEN_TYPE_STRING: return "TYPE_STRING";
        case TOKEN_TYPE_UNIT: return "TYPE_UNIT";
        case TOKEN_ASSIGN: return "=";
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_MUL: return "*";
        case TOKEN_DIV: return "/";
        case TOKEN_EQ: return "==";
        case TOKEN_NEQ: return "!=";
        case TOKEN_LT: return "<";
        case TOKEN_LTE: return "<=";
        case TOKEN_GT: return ">";
        case TOKEN_GTE: return ">=";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_COLON: return ":";
        case TOKEN_COMMA: return ",";
        case TOKEN_DOT: return ".";
        case TOKEN_SEMICOLON: return ";";
        default: return "UNKNOWN";
    }
}

int main(void) {
    Error err = {SUCCESS, "", 0};

    while (1) {
        Token t = nextToken(&err);
        if (err.code != SUCCESS) {
            printError(&err);
            return (int)err.code;
        }

        printf("L%-3d %-10s", t.lineNumber, tokenName(t.type));
        if (t.type == TOKEN_ID || t.type == TOKEN_STRING_LITERAL) {
            printf(" -> \"%s\"", t.val.stringValue);
            free(t.val.stringValue);
        } else if (t.type == TOKEN_INT_LITERAL) {
            printf(" -> %d", t.val.intValue);
        } else if (t.type == TOKEN_DOUBLE_LITERAL) {
            printf(" -> %g", t.val.doubleValue);
        }
        printf("\n");

        if (t.type == TOKEN_EOF) break;
    }

    return 0;
}

/*
all tests at once:

for f in tests/lex/*.ifj; do
    echo "=== $f ==="
    ./test_lex < "$f"
    echo "-> Exit code: $?"
done

make:
gcc -std=c99 -Wall -Wextra -g -Isrc/core tests/lex/test_lex.c src/lex/lex.c -o test_lex

*/