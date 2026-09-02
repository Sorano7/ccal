#include "ast.h"
#include <stdarg.h>
#include <assert.h>

const char *op_to_str[] = {OPS(AS_STR)};

static Expr *expr_new(ExprKind kind, Span span)
{
    Expr *e = malloc(sizeof(Expr));
    assert(e);

    memset(e, 0, sizeof(*e));
    e->kind = kind;
    e->span = span;
    return e;
}

void expr_free(Expr *e)
{
    if (!e) return;
    switch (e->kind)
    {
        case EXPR_ERROR:
            str_free(&e->as.err);
            break;

        case EXPR_NUMBER:
            mpq_clear(e->as.number);
            break;

        case EXPR_INFIX:
            expr_free(e->as.infix.left);
            expr_free(e->as.infix.right);
            break;

        case EXPR_PREFIX:
            expr_free(e->as.prefix.expr);
            break;

        default:
            break;
    }
}

void expr_destroy(Expr **ep)
{
    if (!ep || !*ep) return;
    expr_free(*ep);
    free(*ep);
}


Expr *expr_err(Span span, const char *fmt, ...)
{
    Expr *e = expr_new(EXPR_ERROR, span);
    str_init(&e->as.err);

    va_list args;
    va_start(args, fmt);
    str_appendvf(&e->as.err, fmt, args);
    va_end(args);

    return e;
}

Expr *expr_number(Span span)
{
    Expr *e = expr_new(EXPR_NUMBER, span);
    mpq_init(e->as.number);
    return e;
}

Expr *expr_id(Span span, StringView id)
{
    Expr *e = expr_new(EXPR_IDENT, span);
    e->as.id = id;
    return e;
}

Expr *expr_infix(Expr *l, Operator op, Expr *r)
{
    Span span = {l->span.from, r->span.to};
    Expr *e = expr_new(EXPR_INFIX, span);
    e->as.infix.left = l;
    e->as.infix.op = op;
    e->as.infix.right = r;
    return e;
}

Expr *expr_prefix(Span start, Operator op, Expr *expr)
{
    Span span = {start.from, expr->span.to};
    Expr *e = expr_new(EXPR_PREFIX, span);
    e->as.prefix.op = op;
    e->as.prefix.expr = expr;
    return e;
}

Expr *expr_assign(Span start, StringView id, Expr *expr)
{
    Span span = {start.from, expr->span.to};
    Expr *e = expr_new(EXPR_ASSIGN, span);
    e->as.assign.id = id;
    e->as.assign.expr = expr;
    return e;
}

