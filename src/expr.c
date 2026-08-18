#include "expr.h"
#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

// Corresponding symbols of the operator.
static char *op_symbols[] = {
    [OP_NONE] = "",
    [OP_ADD]  = "+",
    [OP_SUB]  = "-",
    [OP_MUL]  = "*",
    [OP_DIV]  = "/",
    [OP_NEG]  = "-",
};

// Allocate a new expression with kind.
// Abort if out of memory.
Expr *expr_new(ExprKind kind)
{
    Expr *e = malloc(sizeof(Expr));
    assert(e);
    e->kind = kind;
    return e;
}

// Clears and frees the content of an expression.
void expr_free(Expr *e)
{
    if (!e) return;
    switch (e->kind)
    {
        case EXPR_NUMBER:
            mpq_clear(e->as.number.value);
            break;

        case EXPR_INFIX:
            expr_free(e->as.infix.left);
            e->as.infix.left = NULL;
            expr_free(e->as.infix.right);
            e->as.infix.right = NULL;
            break;

        case EXPR_PREFIX:
            expr_free(e->as.prefix.expr);
            e->as.prefix.expr = NULL;
            break;

        default:
            break;
    }
}

// Create an error expression with message.
Expr *expr_error(const char *msg)
{
    Expr *e = expr_new(EXPR_ERROR);
    e->as.error.msg = msg;
    return e;
}

// Check if an expression is error.
bool is_error(Expr *e)
{
    return e->kind == EXPR_ERROR;
}

// Create a number expression.
Expr *expr_number(mpq_t value)
{
    Expr *e = expr_new(EXPR_NUMBER);
    mpq_init(e->as.number.value);

    mpq_set(e->as.number.value, value);
    return e;
}

// Create an infix expression.
Expr *expr_infix(Expr *l, Operator op, Expr *r)
{
    Expr *e = expr_new(EXPR_INFIX);
    e->as.infix.left = l;
    e->as.infix.op = op;
    e->as.infix.right = r;
    return e;
}

// Create a prefix expression.
Expr *expr_prefix(Operator op, Expr *expr)
{
    Expr *e = expr_new(EXPR_PREFIX);
    e->as.prefix.op = op;
    e->as.prefix.expr = expr;
    return e;
}

#define printf_indent(indent, fmt, ...) do { \
    for (int i = 0; i < indent; i++) \
        printf("  "); \
    printf(fmt __VA_OPT__(,) __VA_ARGS__); \
} while (0)

// Print the AST of an expression.
void ast_print(Expr *e, int indent)
{
    if (!e)
    {
        printf("(empty)");
        return;
    }

    printf("{\n");
    indent++;
    switch (e->kind)
    {
        case EXPR_NUMBER:
            printf_indent(indent, "type: number,\n");
            printf_indent(indent, "value: ");
            gmp_printf("%Qd,\n", e->as.number.value);
            break;

        case EXPR_INFIX:
            printf_indent(indent, "type: infix,\n");
            printf_indent(indent, "left: ");
            ast_print(e->as.infix.left, indent);
            printf_indent(indent, "op: %s\n", op_symbols[e->as.infix.op]);
            printf_indent(indent, "right: ");
            ast_print(e->as.infix.right, indent);
            break;

        case EXPR_PREFIX:
            printf_indent(indent, "type: prefix,\n");
            printf_indent(indent, "op: %s\n", op_symbols[e->as.prefix.op]);
            printf_indent(indent, "expr: ");
            ast_print(e->as.prefix.expr, indent);
            break;

        case EXPR_ERROR:
            printf_indent(indent, "type: error,\n");
            printf_indent(indent, "msg: %s\n", e->as.error.msg);
            break;
    }
    indent--;
    printf_indent(indent, "},\n");
}
