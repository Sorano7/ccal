#include "parser.h"
#include <string.h>

// Cast the token as an infix operator.
static Operator token_as_infix_op(Token t)
{
    switch (t.kind)
    {
        case TOK_PLUS:  return OP_ADD;
        case TOK_MINUS: return OP_SUB;
        case TOK_STAR:  return OP_MUL;
        case TOK_SLASH: return OP_DIV;
        default:        return OP_NONE;
    }
}

// Cast the token as a prefix operator.
static Operator token_as_prefix_op(Token t)
{
    switch (t.kind)
    {
        case TOK_MINUS: return OP_NEG;
        default:        return OP_NONE;
    }
}

// Get the precedence of the token.
static OpPrec token_get_prec(Token t)
{
    switch (t.kind)
    {
        case TOK_PLUS:
        case TOK_MINUS:
            return PREC_SUM;

        case TOK_STAR:
        case TOK_SLASH:
            return PREC_PRODUCT;

        default:
            return PREC_PRIMARY;
    }
}

// Intiialize a parser with a reference to the lexer.
void parser_init(Parser *p, const TokenArray *ta)
{
    p->ta = ta;
    p->pos = 0;
}

// Reset the parser. Does not clear lexer reference.
void parser_reset(Parser *p)
{
    p->pos = 0;
}

// Get the current token of the parser.
static Token parser_get_token(const Parser *p)
{
    return p->ta->data[p->pos];
}

// Advance the parser.
static void parser_advance(Parser *p)
{
    if (p->pos >= p->ta->len)
    {
        p->pos = p->ta->len-1;
        return;
    }
    p->pos++;
}

// Checks if the parser is at EOF.
static bool parser_is_eof(Parser *p)
{
    return parser_get_token(p).kind == TOK_EOF;
}

// Checks if the current token matches the token kind.
static bool parser_expect(const Parser *p, TokenKind tk)
{
    Token t = parser_get_token(p);
    return t.kind == tk;
}

// Checks if the current token is a number.
static bool parser_expect_number(const Parser *p)
{
    Token t = parser_get_token(p);
    return t.kind == TOK_LBRAC 
        || t.kind == TOK_ALNUM || t.kind == TOK_DIGIT;
}

// Get the current precedence.
static int parser_get_prec(const Parser *p)
{
    return token_get_prec(parser_get_token(p));
}

// Create an error expression with parser context.
static Expr *parser_error(Parser *p, const char *msg)
{
    Expr *err = expr_error(msg);
    err->pos = parser_get_token(p).pos;
    return err;
}

#define ENSURE_NOT_ERR(e) do { \
    if (is_error(e)) return e; \
} while (0)

// Parse an infix expression.
static Expr *parse_infix_expr(Parser *p, Expr *left, int prec)
{
    Operator op = token_as_infix_op(parser_get_token(p));
    if (op == OP_NONE)
        return parser_error(p, "Unknown infix operator");
    parser_advance(p);

    Expr *right = parse_expr(p, prec);
    ENSURE_NOT_ERR(right);

    return expr_infix(left, op, right);
}

// Parse a number literal.
static Expr *parse_number(Parser *p)
{
    Token tok = parser_get_token(p);

    mpq_t v;
    mpq_init(v);

    char buffer[tok.len+1];
    memcpy(buffer, tok.data, tok.len);
    buffer[tok.len] = '\0';

    if (mpq_set_str(v, buffer, 10) != 0)
        return parser_error(p, "Invalid number literal");

    Expr *e = expr_number(v);
    mpq_clear(v);

    parser_advance(p);
    return e;
}

// Parse a group expression.
static Expr *parse_group(Parser *p)
{
    parser_advance(p);
    Expr *e = parse_expr(p, PREC_PRIMARY);
    ENSURE_NOT_ERR(e);

    if (!parser_expect(p, TOK_RPAREN))
        return parser_error(p, "Expected ')'");

    parser_advance(p);
    return e;
}

// Parse a prefix expression.
static Expr *parse_prefix(Parser *p)
{
    Operator op = token_as_prefix_op(parser_get_token(p));
    if (op == OP_NONE)
        return parser_error(p, "Unknown infix operator");

    parser_advance(p);
    if (!parser_expect(p, TOK_LPAREN) && !parser_expect_number(p))
    {
        return parser_error(p, "Expected number or group");
    }
    Expr *e = parse_expr(p, PREC_PREFIX);
    ENSURE_NOT_ERR(e);

    e = expr_prefix(op, e);
    return e;
}

// Parse an expression.
Expr *parse_expr(Parser *p, int prec)
{
    Expr *e = NULL;

    Token tok = parser_get_token(p);

    switch (tok.kind)
    {
        case TOK_EOF:    return parser_error(p, "Unexpected EOF");

        case TOK_DIGIT:
        case TOK_ALNUM:  e = parse_number(p); break;

        case TOK_LPAREN: e = parse_group(p);  break;

        case TOK_MINUS:  e = parse_prefix(p); break;

        case TOK_LBRAC:  return parser_error(p, "Digit list not implemented");
        default:         return parser_error(p, "Expected expression");
    }

    int current_prec = parser_get_prec(p);

    while (current_prec != PREC_PRIMARY && current_prec > prec)
    {
        if (parser_is_eof(p)) break;
        e = parse_infix_expr(p, e, current_prec);
        ENSURE_NOT_ERR(e);

        current_prec = parser_get_prec(p);
    }

    return e;
}

