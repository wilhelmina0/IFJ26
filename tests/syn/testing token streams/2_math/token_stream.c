Token stream2_math[] = {
    // import ifj26.* \n
    {TOKEN_KW_IMPORT, .lineNumber = 1}, {TOKEN_ID, .val.stringValue = "ifj26", .lineNumber = 1}, {TOKEN_DOT, .lineNumber = 1}, {TOKEN_MUL, .lineNumber = 1}, {TOKEN_EOL, .lineNumber = 1},

    // @main def math(): Unit = \n
    {TOKEN_ANNOTATION_MAIN, .lineNumber = 2}, {TOKEN_KW_DEF, .lineNumber = 2}, {TOKEN_ID, .val.stringValue = "math", .lineNumber = 2}, {TOKEN_LPAREN, .lineNumber = 2}, {TOKEN_RPAREN, .lineNumber = 2}, {TOKEN_COLON, .lineNumber = 2}, {TOKEN_TYPE_UNIT, .lineNumber = 2}, {TOKEN_ASSIGN, .lineNumber = 2}, {TOKEN_EOL, .lineNumber = 2},

    //     val a = 5 + 3 * 2 \n
    {TOKEN_INDENT, .lineNumber = 3},
    {TOKEN_KW_VAL, .lineNumber = 3},
    {TOKEN_ID, .val.stringValue = "a", .lineNumber = 3},
    {TOKEN_ASSIGN, .lineNumber = 3},
    {TOKEN_INT_LITERAL, .val.intValue = 5, .lineNumber = 3},
    {TOKEN_PLUS, .lineNumber = 3},
    {TOKEN_INT_LITERAL, .val.intValue = 3, .lineNumber = 3},
    {TOKEN_MUL, .lineNumber = 3},
    {TOKEN_INT_LITERAL, .val.intValue = 2, .lineNumber = 3},
    {TOKEN_EOL, .lineNumber = 3},
    {TOKEN_DEDENT, .lineNumber = 4},

    {TOKEN_EOF, .lineNumber = 4}
};