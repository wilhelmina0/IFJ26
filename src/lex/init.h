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

#endif //INIT_H
