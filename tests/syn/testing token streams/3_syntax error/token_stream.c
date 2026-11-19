Token stream3_syn_error[] = {
    // import ifj26.* \n
    {TOKEN_KW_IMPORT, .lineNumber = 1}, {TOKEN_ID, .val.stringValue = "ifj26", .lineNumber = 1}, {TOKEN_DOT, .lineNumber = 1}, {TOKEN_MUL, .lineNumber = 1}, {TOKEN_EOL, .lineNumber = 1},

    // @main run(): Unit = \n  <-- MISSING TOKEN_KW_DEF HERE!
    {TOKEN_ANNOTATION_MAIN, .lineNumber = 2}, 
    {TOKEN_ID, .val.stringValue = "run", .lineNumber = 2}, // The parser should immediately flag this as a SYN_ERROR
    {TOKEN_LPAREN, .lineNumber = 2}, 
    {TOKEN_RPAREN, .lineNumber = 2}, 
    {TOKEN_COLON, .lineNumber = 2}, 
    {TOKEN_TYPE_UNIT, .lineNumber = 2}, 
    {TOKEN_ASSIGN, .lineNumber = 2}, 
    {TOKEN_EOL, .lineNumber = 2},

    {TOKEN_INDENT, .lineNumber = 3}, {TOKEN_LPAREN, .lineNumber = 3}, {TOKEN_RPAREN, .lineNumber = 3}, {TOKEN_EOL, .lineNumber = 3}, {TOKEN_DEDENT, .lineNumber = 4},
    {TOKEN_EOF, .lineNumber = 4}
};