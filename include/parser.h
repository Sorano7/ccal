#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "expr.h"
#include <gmp.h>

// Parser for parsing tokens into AST.
typedef struct
{
    const Lexer *lex;
    size_t pos;
} Parser;


void parser_init(Parser *p, const Lexer *l);
void parser_reset(Parser *p);

void parser_diagnostics(Parser *p, Expr *err);

Expr *parse_expr(Parser *p, int prec);

#endif
