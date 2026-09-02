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
    X(OP_NIL, "") \
 \
    X(OP_ADD, "+") \
    X(OP_SUB, "-") \
    X(OP_MUL, "*") \
    X(OP_DIV, "/") \
    X(OP_POW, "^") \
 \
    X(OP_NEG, "-") \
 \
    X(OP_EQ , "==")\
    X(OP_NEQ, "!=") \
 \
    X(OP_LT , "<")\
    X(OP_LEQ, "<=") \
    X(OP_GT , ">")\
    X(OP_GEQ, ">=") \

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

    EXPR_ASSIGN,
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

        struct
        {
            StringView id;
            struct Expr *expr;
        } assign;

    } as;

    Span span;
    ExprKind kind;
} Expr;

void expr_free(Expr *e);
void expr_destroy(Expr **ep);

#define is_error(e) (e->kind == EXPR_ERROR)

Expr *expr_err(Span span, const char *fmt, ...);
Expr *expr_number(Span span);
Expr *expr_id(Span span, StringView id);
Expr *expr_infix(Expr *l, Operator op, Expr *r);
Expr *expr_prefix(Span start, Operator op, Expr *expr);
Expr *expr_assign(Span start, StringView id, Expr *expr);


typedef struct
{
    Expr **data;
    size_t len, cap;
} Module;

void module_free(Module *m);

#endif
