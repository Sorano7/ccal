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
    X(OP_ASSIGN, "=") \
    X(OP_APPLY,  " ") \
    X(OP_PIPE,   "$")

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

    EXPR_LAMBDA,
} ExprKind;

typedef struct Expr
{
    union
    {
        String err;

        mpq_t number;

        String id;

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
            struct Expr *param;
            struct Expr *body;
        } lambda;
    } as;

    Span span;
    ExprKind kind;
} Expr;

void expr_free(Expr *e);
void expr_destroy(Expr **ep);

#define is_error(e) (e->kind == EXPR_ERROR)

Expr *expr_err(Span span, const char *fmt, ...);
Expr *expr_number(Span span);
Expr *expr_number_ui(Span span, unsigned long num, unsigned long den);
Expr *expr_id(Span span, StringView id);
Expr *expr_infix(Expr *l, Operator op, Expr *r);
Expr *expr_prefix(Span start, Operator op, Expr *expr);
Expr *expr_lambda(Expr *id, Expr *body);

Expr *expr_clone(const Expr *e);

bool expr_equal(const Expr *a, const Expr *b);
void expr_render(const Expr *e, String *sb);

#endif
