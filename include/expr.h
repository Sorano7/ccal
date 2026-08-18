#ifndef EXPR_H
#define EXPR_H

#include <stddef.h>
#include <gmp.h>

// Infix and prefix operators.
typedef enum
{
    OP_NONE,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_NEG,
} Operator;

// Operator precedence levels.
typedef enum
{
    PREC_PRIMARY,
    PREC_SUM,
    PREC_PRODUCT,
    PREC_PREFIX,
} OpPrec;

// Kinds of an expression.
typedef enum
{
    EXPR_ERROR,
    EXPR_NUMBER,
    EXPR_INFIX,
    EXPR_PREFIX,
} ExprKind;

// An expression.
typedef struct Expr
{
    union
    {
        struct
        {
            mpq_t value;
        } number;

        struct
        {
            struct Expr *left;
            Operator op;
            struct Expr *right;

        } infix;

        struct
        {
            Operator op;
            struct Expr *expr;
        } prefix;

        struct
        {
            const char *msg;
        } error;
    } as;
    ExprKind kind;

    // The start position of the span.
    // TODO: use a shared span structure across tokens and expressions.
    size_t start;
} Expr;

Expr *expr_new(ExprKind kind);
void expr_free(Expr *e);

Expr *expr_error(const char *msg);
bool is_error(Expr *e);

Expr *expr_number(mpq_t value);
Expr *expr_infix(Expr *l, Operator op, Expr *r);
Expr *expr_prefix(Operator op, Expr *expr);
void ast_print(Expr *e, int indent);

#endif
