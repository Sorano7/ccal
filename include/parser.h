#ifndef PARSER_H
#define PARSER_H

#include "ast.h"

// Parser an expression.
Expr *parse(StringView src, unsigned long base);

#endif
