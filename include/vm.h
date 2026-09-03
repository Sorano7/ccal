#ifndef VM_H
#define VM_H

#include "ast.h"
#include <gmp.h>

typedef struct Scope Scope;

// Kinds of a value.
typedef enum
{
    VAL_VOID,
    VAL_ERROR,
    VAL_NUMBER,
    VAL_BOOL,
    VAL_LAMBDA,
} ValueKind;

// A value that an expression can evaluate to.
typedef struct
{
    union
    {
        String error;

        mpq_t number;

        bool boolean;

        struct
        {
            Expr *expr;
            Scope *env;
        } lambda;
    } as;

    Span span;
    ValueKind kind;
} Value;

// A symbol to value binding.
typedef struct
{
    String *id;
    Value value;
} Symbol;

// An environment scope.
typedef struct Scope
{
    Symbol *data;
    size_t len;
    size_t cap;
    struct Scope *parent;
} Scope;

typedef enum
{
    NUMBER_DECIMAL,
    NUMBER_RATIONAL,
} NumberForm;

typedef struct
{
    StringView src;
    NumberForm num_form;
    unsigned long max_digits;
    unsigned long base;
    bool use_color;
} RenderCtx;

// A VM for just-in-time evaluation.
typedef struct
{
    unsigned long base;
    Value *last;
    Scope *scope;
} VM;

void vm_init(VM *v);
void vm_reset(VM *v);
void vm_free(VM *v);

bool vm_run(VM *v, StringView src, Value *out);

bool vm_eval_expr(VM *v, Expr *e, Value *out);

void vm_value_free(Value *v);

void vm_value_render(Value *v, String *sb, RenderCtx *ctx);
void vm_env_render(VM *v, String *sb, RenderCtx *ctx);

#endif
