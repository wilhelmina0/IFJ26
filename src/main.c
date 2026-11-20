/**
 * @file main.c
 * @brief Main
 *
 * TBD
 *
 * @authors
 * - Tadeáš Vykopal (xvykopt00)
 *
 * @date 2026
 */

#include "lex/init.h"
#include "syn/init.h"
#include "sem/init.h"
//#include "sem/semantics.h" //will dissapear soon
#include "sem/symtable.h"
#include "gen/init.h"
#include "core/error.h"
#include "core/token.h"

int main(void)
{
    Parser parser;
    Node *program = NULL;

    //initialize parser and obtain the first token
    parserInit(&parser);

    //perform syntactical analysis if lexical analysis succeeded
    if (parser.error.code == SUCCESS) {
        program = parseProgram(&parser);
    }
    
    //detect parser failures that were not reported correctly
    if (program == NULL && parser.error.code == SUCCESS) {
        handleError(&parser.error, INTERNAL_ERROR, "Parser returned NULL without reporting an error", parser.currentToken.lineNumber);
    }

    //do semantics
    if (parser.error.code == SUCCESS) {
        SemanticError semErr = {SUCCESS, 0, ""};
        ErrorCode semCode = semanticsRun(program, &semErr);
        
        if (semCode != SUCCESS) {
            handleError( &parser.error, semCode, semErr.message ? semErr.message : "Semantic or code generation error", semErr.line);
        }
    }

    if (parser.error.code == SUCCESS) {
        codeGen(*program);
    }

    //report errors to stderr
    if (parser.error.code != SUCCESS) {
        printError(&parser.error);
    }

    //release allocated resources
    if (program != NULL) {
        destroyAST(program);
    }

    synClean(&parser);

    return (int)parser.error.code;
}