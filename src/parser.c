#include "parser.h"
#include "lexer.h"
#include "number.h"
#include "value.h"

#include <errno.h>

// Operator precedence levels.
typedef enum
{
    PREC_PRIMARY,
    PREC_PIPE,

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
        case TOK_APPROX:
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
        case TOK_PERCENT:
            return PREC_PRODUCT;

        case TOK_CARET:
            return PREC_POWER;

        case TOK_ASSIGN:
            return PREC_ASSIGN;

        case TOK_DOLLAR:
            return PREC_PIPE;

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
        case OP_APPROX:
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
#define tkind(p)    token(p).kind
#define tspan(p)    token(p).span
#define is_alnum(p) (tkind(p) == TOK_ALNUM || tkind(p) == TOK_DIGIT)
#define is_dlist(p) (tkind(p) == TOK_LBRAC)
#define is_sexpr(p) (is_alnum(p) || is_dlist(p) || tkind(p) == TOK_LPAREN)

#define MUST_CONSUME(p, k) do { \
    if (tkind(p) != k) \
        return expr_err(tspan(p), "Expected '%s'", tk_to_str[k]); \
    (p)->pos++; \
} while (0)

#define SHOULD_CONSUME(p, k) do { \
    if (tkind(p) != (k)) \
        return expr_incomplete(tspan(p)); \
    (p)->pos++; \
} while (0)

#define skip_newlines(p) do { \
    while (tkind(p) == TOK_NEWLINE) \
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
            return true;

        case TOK_MINUS:
            return !token(p).ws_suffix;

        default:
            return false;
    }
}

static inline Token next_real_token(Parser *p)
{
    size_t prev = p->pos;
    skip_newlines(p);
    Token t = token(p);
    p->pos = prev;
    return t;
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
    s.from += res.pos + (has_base ? 2 : 0);
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

    MUST_CONSUME(p, TOK_LBRAC);
        for (;;)
        {
            t = token(p);
            if (tkind(p) != TOK_DIGIT)
                return expr_err(t.span, "Not a digit");

            unsigned long val;
            if (!token_to_ul(t, &val) || val >= p->base)
                return expr_err(t.span, "Digit out of bounds for base %lu", p->base);

            da_append(ds, val);
            p->pos++;

            if (tkind(p) == TOK_RBRAC) break;
            MUST_CONSUME(p, TOK_COMMA);
        }
        out->to = tspan(p).to;
    SHOULD_CONSUME(p, TOK_RBRAC);
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
            UNREACHABLE();
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
    Span s = tspan(p);

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

    MUST_CONSUME(p, TOK_DIGIT);
    MUST_CONSUME(p, TOK_HASH);

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

// Parse a negation expression.
static Expr *parse_neg(Parser *p)
{
    Span s = tspan(p);
    MUST_CONSUME(p, TOK_MINUS);

    if (!is_sexpr(p))
        return expr_err(tspan(p), "Expected number or group");

    Expr *e = parse_expr(p, PREC_PREFIX);
    if (!expr_ok(e)) return e;

    return expr_prefix(s, OP_NEG, e);
}

// Parse an identifier
static Expr *parse_ident(Parser *p)
{
    Token t = token(p);
    MUST_CONSUME(p, TOK_ID);
    return expr_id(t.span, SV(t.value));
}

static Expr *parse_lambda(Parser *p);

static Expr *parse_lambda_or_expr(Parser *p, int prec)
{
    if (peek(p, 1).kind == TOK_COLON)
        return parse_lambda(p);
    return parse_expr(p, prec);
}

static Expr *parse_guards(Parser *p)
{
    Expr *out = NULL;
    bool has_else = false;

    for (;;)
    {
        MUST_CONSUME(p, TOK_BAR);

        Expr *cond = parse_expr(p, PREC_PRIMARY);
        if (!expr_ok(cond))
        {
            if (out) expr_destroy(&out);
            return cond;
        }

        MUST_CONSUME(p, TOK_ARROW);
        skip_newlines(p);

        Expr *then = parse_expr(p, PREC_PRIMARY);
        if (!expr_ok(then))
        {
            if (out) expr_destroy(&out);
            return then;
        }

        if (builtin_kind(cond) == BUILTIN_HOLE)
        {
            has_else = true;
            expr_destroy(&cond);
            expr_guard_add(out, NULL, then);
        }

        out = expr_guard_add(out, cond, then);

        skip_newlines(p);
        if (tkind(p) != TOK_BAR) break;
    }

    if (!out || !has_else) 
    {
        if (out) expr_destroy(&out);
        return expr_incomplete(tspan(p));
    }
    return out;
}

// Parse a lambda expression.
static Expr *parse_lambda(Parser *p)
{
    Expr *id = parse_ident(p);
    if (!expr_ok(id)) return id;

    MUST_CONSUME(p, TOK_COLON);
    skip_newlines(p);

    Expr *body = NULL;
    if (tkind(p) == TOK_BAR)
        body = parse_guards(p);
    else
        body = parse_lambda_or_expr(p, PREC_PRIMARY);
    if (!expr_ok(body)) return body;

    return expr_lambda(id, body);
}

// Parse a group expression.
static Expr *parse_group(Parser *p)
{
    MUST_CONSUME(p, TOK_LPAREN);
    skip_newlines(p);

    Expr *e = parse_lambda_or_expr(p, PREC_PRIMARY);
    if (!expr_ok(e)) return e;

    skip_newlines(p);
    SHOULD_CONSUME(p, TOK_RPAREN);
    return e;
}

// Parse a null denotation expression.
static Expr *parse_nud(Parser *p)
{
    if (peek(p, 1).kind == TOK_HASH)
        return parse_base_tag(p);

    if (is_alnum(p))
        return parse_number(p, DIGIT_FMT_ALNUM);

    if (is_dlist(p))
        return parse_number(p, DIGIT_FMT_LIST);

    switch (tkind(p))
    {
        case TOK_ID:     return parse_ident(p);
        case TOK_LPAREN: return parse_group(p);
        case TOK_MINUS:  return parse_neg(p);
        default:         return expr_err(tspan(p),
                                 "Expected expression, got '%s'", tk_to_str[tkind(p)]);
    }
}

// Parse an application expression.
static Expr *parse_apply(Parser *p, Expr *func)
{
    Expr *arg = parse_nud(p);
    if (!expr_ok(arg))
    {
        expr_destroy(&func);
        return arg;
    }
    return expr_infix(func, OP_APPLY, arg);
}

// Parse a conditional expression.
static Expr *parse_cond(Parser *p, Expr *if_)
{
    MUST_CONSUME(p, TOK_QUESTION);

    Expr *then = parse_expr(p, PREC_PRIMARY);
    if (!expr_ok(then)) 
    {
        expr_destroy(&if_);
        return then;
    }

    skip_newlines(p);
    SHOULD_CONSUME(p, TOK_COLON);

    Expr *else_ = parse_expr(p, PREC_PRIMARY);
    if (!expr_ok(else_)) 
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
    if (!expr_ok(right))
    {
        expr_destroy(&f);
        return right;
    }

    return expr_infix(expr_infix(f, OP_APPLY, left), OP_APPLY, right);
}

// Parse a left denotation expression
static Expr *parse_led(Parser *p, int prec, Expr *left)
{
    skip_newlines(p);

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

    skip_newlines(p);

    Expr *right = parse_lambda_or_expr(p, prec);
    if (!expr_ok(right))
    {
        expr_destroy(&left);
        return right;
    }

    return expr_infix(left, op, right);
}

// Parse an expression.
static Expr *parse_expr(Parser *p, int prec)
{
    if (tkind(p) == TOK_EOF)
        return expr_incomplete(tspan(p));

    Expr *e = parse_nud(p);
    if (!expr_ok(e)) return e;

    for (;;)
    {
        if (tkind(p) == TOK_EOF) break;

        if (token(p).ws_prefix && is_nud(p))
        {
            e = parse_apply(p, e);
        }
        else
        {
            Token next = next_real_token(p);
            int next_prec = token_prec(next);
            if (next_prec <= prec) break;
            e = parse_led(p, next_prec, e);
        }
        if (!expr_ok(e)) return e;
    }
    return e;
}

static bool is_empty(Parser *p)
{
    skip_newlines(p);
    return tkind(p) == TOK_EOF;
}

// Parse a single line of one expression with an offset into the source.
Expr *parse_line(StringView line, unsigned long base, size_t offset)
{
    TokenList tl = {0};
    da_init(&tl);

    Parser p = {&tl, 0, base};

    Expr *out = NULL;
    if (!tokenize(p.tl, line, offset))
    {
        Token err = da_last(p.tl);
        DEV_MUST(err.kind == TOK_ERROR);
        out = expr_err(err.span, SV_FMT, SV_ARG(SV(err.value)));
        goto done;
    }

    if (is_empty(&p)) goto done;

    out = parse_expr(&p, PREC_PRIMARY);
    if (expr_ok(out))
    {
        while (tkind(&p) == TOK_SEMICOLON || tkind(&p) == TOK_NEWLINE)
            p.pos++;

        if (tkind(&p) != TOK_EOF)
            out = expr_err(tspan(&p), "Trailing characters");
    }

done:
    token_list_free(p.tl);
    return out;
}

Expr *parse(StringView src, unsigned long base)
{
    return parse_line(src, base, 0);
}
