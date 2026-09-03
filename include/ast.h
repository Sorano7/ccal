#ifndef AST_H
#define AST_H

#include "cut.h"
#include <gmp.h>

typedef struct
{
    size_t from;
    size_t to;
} Span;

#define OPS(X) \
    X(OP_NIL,    "") \
\
    X(OP_ADD,    "+") \
    X(OP_SUB,    "-") \
    X(OP_MUL,    "*") \
    X(OP_DIV,    "/") \
    X(OP_POW,    "^") \
\
    X(OP_NEG,    "-") \
\
    X(OP_EQ ,    "==") \
    X(OP_NEQ,    "!=") \
\
    X(OP_LT ,    "<") \
    X(OP_LEQ,    "<=") \
    X(OP_GT ,    ">") \
    X(OP_GEQ,    ">=") \
\
    X(OP_ASSIGN, "=")

#define AS_ENUM(name, _) name,
#define AS_STR(name, s)  [name] = (s),

typedef enum
{
    OPS(AS_ENUM)
} Operator;

extern const char *op_to_str[];

typedef enum
{
    EXPR_ERROR,

    EXPR_NUMBER,
    EXPR_IDENT,

    EXPR_INFIX,
    EXPR_PREFIX,
} ExprKind;

typedef struct Expr
{
    union
    {
        String err;

        mpq_t number;

        StringView id;

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
    } as;

    Span span;
    ExprKind kind;
} Expr;

// Free an expression recursively.
void expr_free(Expr *e);

// Free an expression's content and itself.
void expr_destroy(Expr **ep);

// Checks if an expression is error.
#define is_error(e) (e->kind == EXPR_ERROR)

// Allocate an error expression with span and message.
Expr *expr_err(Span span, const char *fmt, ...);

// Allocate a number expression with span without setting a value.
Expr *expr_number(Span span);

// Allocate a number expression with value.
Expr *expr_number_ui(Span span, unsigned long num, unsigned long den);

// Allocate an identifier expression with span and name.
Expr *expr_id(Span span, StringView id);

// Allocate an infix expression with span implied by left and right.
Expr *expr_infix(Expr *l, Operator op, Expr *r);

// Allocate an prefix expression with span from start to expr.
Expr *expr_prefix(Span start, Operator op, Expr *expr);

// Check if two expressions are structurally equal.
bool expr_equal(const Expr *a, const Expr *b);

#endif
