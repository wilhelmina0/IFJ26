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

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "../ast.h"

char *codeGen(Node top);

#endif //INIT_H
