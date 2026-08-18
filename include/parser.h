#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "expr.h"
#include <gmp.h>

// Parser for parsing tokens into AST.
typedef struct
{
    const TokenArray *ta;
    size_t pos;
} Parser;

void parser_init(Parser *p, const TokenArray *ta);
void parser_reset(Parser *p);

Expr *parse_expr(Parser *p, int prec);

#endif
