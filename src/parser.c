#include "parser.h"
#include "digit.h"

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#define BASE_DEFAULT 10

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

static size_t parser_find_next(Parser *p, TokenKind tk)
{
    size_t prev_pos = p->pos;
    while (!parser_expect(p, tk))
    {
        parser_advance(p);
        if (parser_is_eof(p))
            return SIZE_MAX;
    }

    size_t d = p->pos - prev_pos;
    p->pos = prev_pos;
    return d;
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

#define ENSURE_DIGIT_OK(p, e) do { \
    if ((e) == DIGIT_INVALID) \
        return parser_error((p), "Invalid digit"); \
    if ((e) == DIGIT_OUT_OF_BASE) \
        return parser_error((p), "Digit out of bounds"); \
} while (0)

static bool token_to_ul(Token tok, unsigned long *out)
{
    char buffer[tok.len+1];
    memcpy(buffer, tok.data, tok.len);
    buffer[tok.len] = '\0';

    errno = 0;

    char *end_ptr = NULL;
    *out = strtoul(buffer, &end_ptr, 10);
    if (errno == ERANGE)
        return false;

    return true;
}

// Try parsing a number part and return any error, NULL if no error is encountered.
static Expr *try_parse_number_part(Parser *p, Digits *ds, DigitFormat fmt, unsigned long base)
{
    Token tok = parser_get_token(p);

    switch (tok.kind)
    {
        case TOK_ALNUM:
        case TOK_DIGIT:
            if (fmt != DIGIT_FMT_ALNUM)
                return parser_error(p, "Expected digit list");

            DigitError err = digits_from_alnum(ds, tok.data, tok.len, base);
            ENSURE_DIGIT_OK(p, err);
            parser_advance(p);
            return NULL;

        case TOK_LBRAC:
            if (fmt != DIGIT_FMT_LIST)
                return parser_error(p, "Expected alphanumerics");

            parser_advance(p);
            size_t i = 0;

            size_t len = parser_find_next(p, TOK_RBRAC);
            if (len == SIZE_MAX)
                return parser_error(p, "Expected ']'");

            digits_init(ds, len);
            for (;;)
            {
                tok = parser_get_token(p);
                if (tok.kind != TOK_DIGIT)
                    return parser_error(p, "Expected numeric digit");

                unsigned long val;
                if (!token_to_ul(tok, &val) || val >= base)
                    return parser_error(p, "Digit out of bounds");
                parser_advance(p);

                ds->data[i++] = val;
                if (parser_expect(p, TOK_RBRAC))
                {
                    parser_advance(p);
                    break;
                }

                if (!parser_expect(p, TOK_COMMA))
                    return parser_error(p, "Expected ','"); 

                parser_advance(p);
            }
            ds->len = i;
            return NULL;

        default:
            return parser_error(p, "Expected number");
    }
}

// Parse a number literal.
static Expr *parse_number(Parser *p, DigitFormat fmt)
{
    unsigned long base = BASE_DEFAULT;

    Literal lit = {0};

    Digits ds_i = {0};
    Digits ds_n = {0};
    Digits ds_r = {0};

    Expr *err = NULL;

    if ((err = try_parse_number_part(p, &ds_i, fmt, base)))
        return err;

    lit.I = &ds_i;

    mpq_t value;
    mpq_init(value);

    if (!parser_expect(p, TOK_DOT)) goto cleanup;

    parser_advance(p);

    if (!parser_expect(p, TOK_LPAREN))
    {
        if ((err = try_parse_number_part(p, &ds_n, fmt, base)))
                return err;

        lit.N = &ds_n;
    }

    if (parser_expect(p, TOK_LPAREN))
    {
        parser_advance(p);
        if ((err = try_parse_number_part(p, &ds_r, fmt, base)))
            return err;

        if (!parser_expect(p, TOK_RPAREN))
            return parser_error(p, "Expected ')'");

        lit.R = &ds_r;
    }

cleanup:
    literal_to_mpq(&lit, base, value);
    Expr *e = expr_number(value);

    digits_free(&ds_i);
    digits_free(&ds_n);
    digits_free(&ds_r);
    mpq_clear(value);
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
        case TOK_EOF:
            return parser_error(p, "Unexpected EOF");

        case TOK_DIGIT:
        case TOK_ALNUM:
             e = parse_number(p, DIGIT_FMT_ALNUM);
             break;

        case TOK_LBRAC:
             e = parse_number(p, DIGIT_FMT_LIST);
             break;

        case TOK_LPAREN:
             e = parse_group(p);
             break;

        case TOK_MINUS:
             e = parse_prefix(p);
             break;

        default:
             return parser_error(p, "Expected expression");
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

