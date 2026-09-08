#ifndef PARSER_H
#define PARSER_H

#include "ast.h"

// Parse an expression.
Expr *parse(StringView src, unsigned long base);

// Parse a module.
bool parse_module(StringView src, unsigned long base, Module *m);

#endif
