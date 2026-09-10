#include "parser.h"
#include "lexer.h"
#include "number.h"

#include <errno.h>

// Operator precedence levels.
typedef enum
{
    PREC_PRIMARY,

    PREC_COND,

    PREC_ASSIGN,

    PREC_EQUALITY,
    PREC_COMPARISON,

    PREC_SUM,
    PREC_PRODUCT,
    PREC_POWER,

    PREC_APPLY,

    PREC_PREFIX,
    PREC_BASE,
} OpPrec;

// Get the precedence of the token.
static OpPrec token_prec(Token t)
{
    switch (t.kind)
    {
        case TOK_QUESTION:
            return PREC_COND;

        case TOK_EQ:
        case TOK_NEQ:
            return PREC_EQUALITY;

        case TOK_LT:
        case TOK_LEQ:
        case TOK_GT:
        case TOK_GEQ:
            return PREC_COMPARISON;

        case TOK_PLUS:
        case TOK_MINUS:
            return PREC_SUM;

        case TOK_STAR:
        case TOK_SLASH:
            return PREC_PRODUCT;

        case TOK_CARET:
            return PREC_POWER;

        case TOK_ASSIGN:
            return PREC_ASSIGN;

        case TOK_DOLLAR:
        case TOK_INFIX_ID:
            return PREC_APPLY;

        default:
            return PREC_PRIMARY;
    }
}

// Return whether the operator is right associative.
static bool is_right_associative(Operator op)
{
    switch (op)
    {
        case OP_EQ:
        case OP_NEQ:
        case OP_LT:
        case OP_LEQ:
        case OP_GT:
        case OP_GEQ:
        case OP_POW:
        case OP_NEG:
        case OP_ASSIGN:
        case OP_PIPE:
            return true;

        default:
            return false;
    }
}

typedef struct
{
    TokenList *tl;
    size_t pos;
    unsigned long base;
} Parser;

#define AT_OR_LAST(p, i) (i) < (p)->tl->len ? (i) : (p)->tl->len-1

#define peek(p, n)  (p)->tl->data[AT_OR_LAST((p), (p)->pos + (n))]
#define token(p)    peek(p, 0)
#define tprec(p)    token_prec(token(p))
#define tkind(p)    token(p).kind
#define tspan(p)    token(p).span
#define is_alnum(p) (tkind(p) == TOK_ALNUM || tkind(p) == TOK_DIGIT)
#define is_dlist(p) (tkind(p) == TOK_LBRAC)
#define is_sexpr(p) (is_alnum(p) || is_dlist(p) || tkind(p) == TOK_LPAREN)

#define CONSUME_EXPECT(p, k) do { \
    if (tkind(p) != k) \
        return expr_err(tspan(p), "Expected '%s'", tk_to_str[k]); \
    (p)->pos++; \
} while (0)

// Return if the next expression is nud.
static inline bool is_nud(Parser *p)
{
    switch (tkind(p))
    {
        case TOK_DIGIT:
        case TOK_LPAREN:
        case TOK_ALNUM:
        case TOK_ID:
        case TOK_LBRAC:
        case TOK_UNDER:
        case TOK_COLON:
            return true;

        case TOK_MINUS:
            return !token(p).ws_suffix;

        default:
            return false;
    }
}

static Expr *parse_expr(Parser *p, int prec);

// Convert a token to an unsigned long value.
// t must be TOK_DIGIT.
static bool token_to_ul(Token t, unsigned long *out)
{
    SV_TO_CSTR(t.value, buffer);
    errno = 0;

    char *end_ptr = NULL;
    *out = strtoul(buffer, &end_ptr, 10);
    return errno != ERANGE;
}

// Parse an alphanumeric number part, return error.
static Expr *parse_number_part_alnum(Parser *p, DigitArray *ds, bool has_base, Span *out)
{
    Token t = token(p);
    Span s = t.span;

    if (!is_alnum(p))
        return expr_err(s, "Expected alphanumerics");

    StringView src = SV(t.value);
    if (has_base) sv_shift(&src, 2);

    DigitResult res = digits_from_alnum(ds, src, p->base);
    s.from += res.pos;
    s.to = s.from+1;

    switch (res.kind)
    {
        case DIGIT_INVALID:
            return expr_err(s, "Not a digit");
        case DIGIT_OOB:
            return expr_err(s, "Digit out of bounds for base %lu", p->base);
        case DIGIT_BASE_TOO_LARGE:
            return expr_err(t.span, "Base too large for alphanumeric spelling");
        default:
            break;
    }
    p->pos++;
    out->to = t.span.to;
    return NULL;
}

// Parse a digit list number part, return error.
static Expr *parse_number_part_dlist(Parser *p, DigitArray *ds, Span *out)
{
    Token t = token(p);
    Span s = t.span;

    if (!is_dlist(p))
        return expr_err(s, "Expected digit list");

    CONSUME_EXPECT(p, TOK_LBRAC);
        for (;;)
        {
            t = token(p);
            if (tkind(p) != TOK_DIGIT)
                return expr_err(t.span, "Expected numeric value as digit");

            unsigned long val;
            if (!token_to_ul(t, &val) || val >= p->base)
                return expr_err(t.span, "Digit out of bounds");

            da_append(ds, val);
            p->pos++;

            if (tkind(p) == TOK_RBRAC) break;
            CONSUME_EXPECT(p, TOK_COMMA);
        }
        out->to = tspan(p).to;
    CONSUME_EXPECT(p, TOK_RBRAC);
    return NULL;
}

// Parse a number part (I, N, or R) into a sequence of digits.
// Ensure the number is in the same format as fmt.
static Expr *parse_number_part(Parser *p, DigitArray *ds, DigitFormat fmt, bool has_base, Span *out)
{
    switch (fmt)
    {
        case DIGIT_FMT_ALNUM:
            return parse_number_part_alnum(p, ds, has_base, out);

        case DIGIT_FMT_LIST:
            return parse_number_part_dlist(p, ds, out);

        default:
            return expr_err(tspan(p), "Expected number");
    }
}

// Try parsing a base prefix in the form of a leading zero.
static bool try_parse_base_prefix(Token *t, unsigned long *base)
{
    if (t->kind != TOK_ALNUM) return false;

    if (sv_startswith(SV(t->value), SV("0x")))
        *base = 16;
    else if (sv_startswith(SV(t->value), SV("0b")))
        *base = 2;
    else if (sv_startswith(SV(t->value), SV("0o")))
        *base = 8;
    else if (sv_startswith(SV(t->value), SV("0d")))
        *base = 10;
    else
        return false;
    return true;
}

// Parse a number literal in the form of I.N(R).
static Expr *parse_number(Parser *p, DigitFormat fmt)
{
    Expr *e = NULL;
    Span s = {0};

    Literal lit;
    literal_init(&lit);

    unsigned long prev_base = p->base;
    bool has_base = try_parse_base_prefix(&token(p), &p->base);

    if ((e = parse_number_part(p, &lit.I, fmt, has_base, &s)))
        goto cleanup;

    if (tkind(p) != TOK_DOT)
        goto eval;
    p->pos++;

    if (tkind(p) != TOK_LPAREN)
    {
        if ((e = parse_number_part(p, &lit.N, fmt, false, &s)))
            goto cleanup;
    }

    if (tkind(p) == TOK_LPAREN)
    {
        p->pos++;
        if ((e = parse_number_part(p, &lit.R, fmt, false, &s)))
            goto cleanup;

        if (tkind(p) != TOK_RPAREN)
        {
            e = expr_err(tspan(p), "Expected ')'");
            goto cleanup;
        }
        p->pos++;
    }

eval:
    e = expr_number(s);
    literal_to_mpq(&lit, p->base, e->as.number);

cleanup:
    literal_free(&lit);
    p->base = prev_base;
    return e;
}

// Parse a single expression in the base denoted by the tag.
static Expr *parse_base_tag(Parser *p)
{
    Token t = token(p);
    Span s = t.span;

    CONSUME_EXPECT(p, TOK_DIGIT);
    CONSUME_EXPECT(p, TOK_HASH);

    unsigned long prev_base = p->base;

    if (!token_to_ul(t, &p->base))
        return expr_err(s, "Base too large");
    if (p->base <= 1)
        return expr_err(s, "Base must be at least 2");
    if (!is_sexpr(p))
        return expr_err(tspan(p), "Base prefix must precede expression");

    Expr *e = parse_expr(p, PREC_BASE);
    p->base = prev_base;
    return e;
}

// Parse a group expression.
static Expr *parse_group(Parser *p)
{
    CONSUME_EXPECT(p, TOK_LPAREN);
    Expr *e = parse_expr(p, PREC_PRIMARY);
    if (is_error(e)) return e;
    CONSUME_EXPECT(p, TOK_RPAREN);
    return e;
}

// Parse a negation expression.
static Expr *parse_neg(Parser *p)
{
    Span s = tspan(p);
    CONSUME_EXPECT(p, TOK_MINUS);

    if (!is_sexpr(p))
            return expr_err(tspan(p), "Expected number of group");

    Expr *e = parse_expr(p, PREC_PREFIX);
    if (is_error(e)) return e;

    return expr_prefix(s, OP_NEG, e);
}

// Parse an identifier
static Expr *parse_ident(Parser *p)
{
    Token t = token(p);
    CONSUME_EXPECT(p, TOK_ID);
    return expr_id(t.span, SV(t.value));
}

// Parse a lambda expression.
static Expr *parse_lambda(Parser *p)
{
    Expr *id = parse_ident(p);
    if (is_error(id)) return id;

    CONSUME_EXPECT(p, TOK_COLON);

    Expr *body = parse_expr(p, PREC_PRIMARY);
    if (is_error(body)) return body;

    return expr_lambda(id, body);
}

// Parse a null denotation expression.
static Expr *parse_nud(Parser *p)
{
    if (peek(p, 1).kind == TOK_HASH)
        return parse_base_tag(p);

    if (peek(p, 1).kind == TOK_COLON)
        return parse_lambda(p);

    if (is_alnum(p))
        return parse_number(p, DIGIT_FMT_ALNUM);

    if (is_dlist(p))
        return parse_number(p, DIGIT_FMT_LIST);

    switch (tkind(p))
    {
        case TOK_ID:     return parse_ident(p);
        case TOK_LPAREN: return parse_group(p);
        case TOK_MINUS:  return parse_neg(p);
        default:         return expr_err(tspan(p), "Expected expression");
    }
}

// Parse an application expression.
static Expr *parse_apply(Parser *p, Expr *func)
{
    Expr *arg = parse_nud(p);
    if (is_error(arg))
    {
        expr_destroy(&func);
        return arg;
    }
    return expr_infix(func, OP_APPLY, arg);
}

// Parse a conditional expression.
static Expr *parse_cond(Parser *p, Expr *if_)
{
    CONSUME_EXPECT(p, TOK_QUESTION);

    Expr *then = parse_expr(p, PREC_PRIMARY);
    if (is_error(then)) 
    {
        expr_destroy(&if_);
        return then;
    }

    CONSUME_EXPECT(p, TOK_BAR);

    Expr *else_ = parse_expr(p, PREC_PRIMARY);
    if (is_error(else_)) 
    {
        expr_destroy(&if_);
        expr_destroy(&then);
        return else_;
    }
    return expr_cond(if_, then, else_);
}

// Parse an infix application of an identifier.
static Expr *parse_infix_apply(Parser *p, Expr *left)
{
    Expr *f = expr_id(tspan(p), SV(token(p).value));
    p->pos++;

    Expr *right = parse_expr(p, PREC_PRIMARY);
    if (is_error(right))
    {
        expr_destroy(&f);
        return right;
    }

    return expr_infix(expr_infix(f, OP_APPLY, left), OP_APPLY, right);
}

// Parse a left denotation expression
static Expr *parse_led(Parser *p, int prec, Expr *left)
{
    if (tkind(p) == TOK_QUESTION)
        return parse_cond(p, left);

    if (tkind(p) == TOK_INFIX_ID)
        return parse_infix_apply(p, left);

    Operator op = token_to_op(token(p));
    switch (op)
    {
        case OP_NIL:
            return expr_err(tspan(p), "Unknown operator");
        case OP_ASSIGN:
            if (left->kind != EXPR_IDENT)
                return expr_err(left->span, "Expected identifier");
            break;
        default:
            break;
    }
    p->pos++;

    if (is_right_associative(op))
        prec--;

    Expr *right = parse_expr(p, prec);
    if (is_error(right))
    {
        expr_destroy(&left);
        return right;
    }

    return expr_infix(left, op, right);
}

// Parse an expression.
static Expr *parse_expr(Parser *p, int prec)
{
    Expr *e = parse_nud(p);
    if (is_error(e)) return e;

    for (;;)
    {
        if (tkind(p) == TOK_EOF) break;

        if (token(p).ws_prefix && is_nud(p))
        {
            e = parse_apply(p, e);
        }
        else
        {
            if ((int)tprec(p) <= prec) break;
            e = parse_led(p, tprec(p), e);
        }
        if (is_error(e)) return e;
    }
    return e;
}

// Parse a module.
bool parse_module(StringView src, unsigned long base, Module *m)
{
    Span s = {0, 0};
    if (base == 0) base = BASE_DEFAULT;
    if (base == 1) return expr_err(s, "Base must be at least 2");

    bool ok = true;

    TokenList tl = {0};
    da_init(&tl);

    Expr *e = NULL;

    if (!tokenize(&tl, src))
    {
        Token err = da_last(&tl);
        if (err.kind == TOK_ERROR)
        {
            e = expr_err(err.span, SV_FMT, SV_ARG(SV(err.value)));
            da_append(m, e);
        }
        ok = false;
        goto cleanup;
    }

    Parser p = {
        .tl = &tl,
        .pos = 0,
        .base = base,
    };

    for (;;)
    {
        e = parse_expr(&p, PREC_PRIMARY);
        da_append(m, e);

        if (is_error(e))
        {
            ok = false;
            goto cleanup;
        }

        TokenKind tk = tkind(&p);
        if (tk == TOK_EOF) break;

        if (tk != TOK_NEWLINE && tk != TOK_SEMICOLON)
        {
            e = expr_err(tspan(&p), "Trailing characters");
            da_append(m, e);
            ok = false;
            goto cleanup;
        }

        for (; tk == TOK_NEWLINE || tk == TOK_SEMICOLON; tk = tkind(&p))
            p.pos++;
    }

cleanup:
    token_list_free(&tl);
    return ok;
}

// Parse an expression.
Expr *parse(StringView src, unsigned long base)
{
    Module m;
    da_init(&m);

    parse_module(src, base, &m);

    Expr *e = expr_clone(da_last(&m));
    module_free(&m);
    return e;
}

