#ifndef PARSER_H
#define PARSER_H

#include "ast.h"

Expr *parse_line(StringView line, unsigned long base, size_t offset);
Expr *parse(StringView src, unsigned long base);

#endif
