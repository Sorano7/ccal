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

        case EXPR_IDENT:
            str_free(&e->as.id);
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
    str_init_with(&e->as.id, id);
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

// Allocate a lambda expression.
Expr *expr_lambda(Expr *id, Expr *body)
{
    Span span = {id->span.from, body->span.to};
    Expr *e = expr_new(EXPR_LAMBDA, span);
    e->as.lambda.param = id;
    e->as.lambda.body = body;
    return e;
}

// Allocate a conditional expression.
Expr *expr_cond(Expr *if_, Expr *then, Expr *else_)
{
    Span span = {if_->span.from, else_->span.to};
    Expr *e = expr_new(EXPR_COND, span);
    e->as.cond.if_ = if_;
    e->as.cond.then = then;
    e->as.cond.else_ = else_;
    return e;
}

// Create a deep clone of the expression.
Expr *expr_clone(const Expr *e)
{
    Expr *out = expr_new(e->kind, e->span);
    switch (e->kind)
    {
        case EXPR_IDENT:
            str_init_with(&out->as.id, &e->as.id);
            break;

        case EXPR_ERROR:
            str_init_with(&out->as.err, &e->as.err);
            break;

        case EXPR_NUMBER:
            mpq_init(out->as.number);
            mpq_set(out->as.number, e->as.number);
            break;

        case EXPR_INFIX:
            out->as.infix.left = expr_clone(e->as.infix.left);
            out->as.infix.op = e->as.infix.op;
            out->as.infix.right = expr_clone(e->as.infix.right);
            break;

        case EXPR_PREFIX:
            out->as.prefix.op = e->as.prefix.op;
            out->as.prefix.expr = expr_clone(e->as.prefix.expr);
            break;

        case EXPR_LAMBDA:
            out->as.lambda.param = expr_clone(e->as.lambda.param);
            out->as.lambda.body = expr_clone(e->as.lambda.body);
            break;

        case EXPR_COND:
            out->as.cond.if_ = expr_clone(e->as.cond.if_);
            out->as.cond.then = expr_clone(e->as.cond.then);
            out->as.cond.else_ = expr_clone(e->as.cond.else_);
            break;

        default:
            UNREACHABLE();
    }
    return out;
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

        case EXPR_LAMBDA:
            if (!expr_equal(a->as.lambda.param, b->as.lambda.param))
                return false;
            return expr_equal(a->as.lambda.body, b->as.lambda.body);

        case EXPR_COND:
            if (!expr_equal(a->as.cond.if_, b->as.cond.if_))
                return false;
            if (!expr_equal(a->as.cond.then, b->as.cond.then))
                return false;
            return expr_equal(a->as.cond.else_, b->as.cond.else_);

        default:
            UNREACHABLE();
    }
}

static void op_render(Operator op, String *sb)
{
    const char *ops = op_to_str[op];
    switch (op)
    {
        case OP_NEG:
            str_appendf(sb, " %s", ops);
            break;

        case OP_APPLY:
            str_append(sb, " ");
            break;

        default:
            str_appendf(sb, " %s ", ops);
    }
}

// Pretty print an expression.
void expr_render(const Expr *e, String *sb)
{
    switch (e->kind)
    {
        case EXPR_IDENT:
            str_appendf(sb, SV_FMT, SV_ARG(e->as.id));
            break;

        case EXPR_NUMBER:
            char *s = mpq_get_str(NULL, 10, e->as.number);
            str_appendf(sb, "%s", s);
            free(s);
            break;

        case EXPR_ERROR:
            str_append(sb, &e->as.err);
            break;

        case EXPR_INFIX:
            str_appendf(sb, "(");
            expr_render(e->as.infix.left, sb);
            op_render(e->as.infix.op, sb);
            expr_render(e->as.infix.right, sb);
            str_appendf(sb, ")");
            break;

        case EXPR_PREFIX:
            str_appendf(sb, "(");
            op_render(e->as.prefix.op, sb);
            expr_render(e->as.prefix.expr, sb);
            str_appendf(sb, ")");
            break;


        case EXPR_LAMBDA:
            str_appendf(sb, "(");
            expr_render(e->as.lambda.param, sb);
            str_appendf(sb, " : ");
            expr_render(e->as.lambda.body, sb);
            str_appendf(sb, ")");
            break;

        case EXPR_COND:
            str_appendf(sb, "(");
            expr_render(e->as.cond.if_, sb);
            str_appendf(sb, " ? ");
            expr_render(e->as.cond.then, sb);
            str_appendf(sb, " | ");
            expr_render(e->as.cond.else_, sb);
            str_appendf(sb, ")");
            break;

        default:
            UNREACHABLE();
    }
}
