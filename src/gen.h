/**
 * @file gen.c
 * @brief generator interface
 *
 * outward facing parts of generator
 *
 * @authors
 * - Josef Kalousek - 279279
 *
 */

#ifndef GEN_INIT_H
#define GEN_INIT_H

#include "ast.h"

char *codeGen(Node top, Error *err);

#endif //INIT_H
