Token stream1_minimal[] = {
    // import ifj26.* \n
    {TOKEN_KW_IMPORT, .lineNumber = 1},
    {TOKEN_ID, .val.stringValue = "ifj26", .lineNumber = 1},
    {TOKEN_DOT, .lineNumber = 1},
    {TOKEN_MUL, .lineNumber = 1},
    {TOKEN_EOL, .lineNumber = 1},

    // @main def run(): Unit = \n
    {TOKEN_ANNOTATION_MAIN, .lineNumber = 2},
    {TOKEN_KW_DEF, .lineNumber = 2},
    {TOKEN_ID, .val.stringValue = "run", .lineNumber = 2},
    {TOKEN_LPAREN, .lineNumber = 2},
    {TOKEN_RPAREN, .lineNumber = 2},
    {TOKEN_COLON, .lineNumber = 2},
    {TOKEN_TYPE_UNIT, .lineNumber = 2},
    {TOKEN_ASSIGN, .lineNumber = 2},
    {TOKEN_EOL, .lineNumber = 2},

    //     () \n
    {TOKEN_INDENT, .lineNumber = 3},
    {TOKEN_LPAREN, .lineNumber = 3},
    {TOKEN_RPAREN, .lineNumber = 3},
    {TOKEN_EOL, .lineNumber = 3},
    {TOKEN_DEDENT, .lineNumber = 4},
    
    {TOKEN_EOF, .lineNumber = 4}
};