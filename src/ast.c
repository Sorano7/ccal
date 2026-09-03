#include "ast.h"
#include <stdarg.h>
#include <assert.h>

const char *op_to_str[] = {OPS(AS_STR)};

// Allocate an expression with kind and span.
static Expr *expr_new(ExprKind kind, Span span)
{
    Expr *e = malloc(sizeof(Expr));
    assert(e);

    memset(e, 0, sizeof(*e));
    e->kind = kind;
    e->span = span;
    return e;
}

// Free an expression recursively.
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

// Free an expression's content and itself.
void expr_destroy(Expr **ep)
{
    if (!ep || !*ep) return;
    expr_free(*ep);
    free(*ep);
}

// Allocate an error expression with span and message.
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

// Allocate a number expression with span without setting a value.
Expr *expr_number(Span span)
{
    Expr *e = expr_new(EXPR_NUMBER, span);
    mpq_init(e->as.number);
    return e;
}

// Allocate a number expression with value.
Expr *expr_number_ui(Span span, unsigned long num, unsigned long den)
{
    Expr *e = expr_number(span);
    mpq_set_ui(e->as.number, num, den);
    return e;
}

// Allocate an identifier expression with span and name.
Expr *expr_id(Span span, StringView id)
{
    Expr *e = expr_new(EXPR_IDENT, span);
    e->as.id = id;
    return e;
}

// Allocate an infix expression with span implied by left and right.
Expr *expr_infix(Expr *l, Operator op, Expr *r)
{
    Span span = {l->span.from, r->span.to};
    Expr *e = expr_new(EXPR_INFIX, span);
    e->as.infix.left = l;
    e->as.infix.op = op;
    e->as.infix.right = r;
    return e;
}

// Allocate an prefix expression with span from start to expr.
Expr *expr_prefix(Span start, Operator op, Expr *expr)
{
    Span span = {start.from, expr->span.to};
    Expr *e = expr_new(EXPR_PREFIX, span);
    e->as.prefix.op = op;
    e->as.prefix.expr = expr;
    return e;
}

// Check if two expressions are structurally equal.
bool expr_equal(const Expr *a, const Expr *b)
{
    if (a->kind != b->kind)
        return false;

    switch (a->kind)
    {
        case EXPR_ERROR:
            return sv_equal(a->as.err, b->as.err);

        case EXPR_IDENT:
            return sv_equal(a->as.id, b->as.id);

        case EXPR_NUMBER:
            return mpq_cmp(a->as.number, b->as.number) == 0;

        case EXPR_PREFIX:
            if (a->as.prefix.op != b->as.prefix.op)
                return false;
            return expr_equal(a->as.prefix.expr, b->as.prefix.expr);

        case EXPR_INFIX:
            if (a->as.infix.op != b->as.infix.op)
                return false;
            if (!expr_equal(a->as.infix.left, b->as.infix.left))
                return false;
            return expr_equal(a->as.infix.right, b->as.infix.right);

        default:
            return false;
    }
}
