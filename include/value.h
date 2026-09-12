#ifndef VALUE_H
#define VALUE_H

#include "cut.h"
#include "ast.h"
#include "creal.h"
#include <gmp.h>

// Kinds of a value.
typedef enum
{
    VAL_VOID,
    VAL_ERROR,
    VAL_EXACT,
    VAL_REAL,
    VAL_LAMBDA,
    VAL_BUILTIN,
} ValueKind;

typedef enum
{
    BUILTIN_NONE,

    BUILTIN_HOLE,
    BUILTIN_ANS,

    BUILTIN_PI,
    BUILTIN_E,

    BUILTIN_TRUE,
    BUILTIN_FALSE,

    BUILTIN_SQRT,

    BUILTIN_POW,
    BUILTIN_EXP,
    BUILTIN_LOG,
    BUILTIN_LN,
} BuiltinKind;

extern const char *vk_to_str[];
extern const char *builtin_to_str[];

typedef struct Value Value;

typedef struct
{
    Value **data;
    size_t len, cap;
} ValueList;

typedef struct Scope Scope;

// A value that an expression can evaluate to.
typedef struct Value
{
    union
    {
        String error;

        struct
        {
            BuiltinKind kind;
            size_t arity;
            ValueList args;
        } builtin;

        mpq_t exact;

        CR *real;

        struct
        {
            Expr *expr;
            Scope *env;
        } lambda;
    } as;

    Span span;
    ValueKind kind;
    size_t refcount;
} Value;

// A symbol to value binding.
typedef struct
{
    String *id;
    Value *value;
} Symbol;

// An environment scope.
typedef struct Scope
{
    Symbol *data;
    size_t len;
    size_t cap;
    size_t refcount;
    struct Scope *parent;
} Scope;

Value *value_void(Span span);
Value *value_exact(Span span, const mpq_t n);
Value *value_real(Span span, CR *n);
Value *value_bool(Span span, bool b);
Value *value_builtin(Span span, BuiltinKind kind, size_t arity);
Value *value_lambda(Expr *e, Scope *s);
Value *value_errorf(Span span, const char *fmt, ...);
Value *value_error_from_expr(const Expr *e);

Value *value_retain(Value *from);
Value *value_clone(const Value *from);
void value_release(Value *v);

#define value_is_err(v) (!v || v->kind == VAL_ERROR)

bool value_is_bool(const Value *v);
bool value_to_bool(const Value *v);

BuiltinKind builtin_kind(Expr *e);

void scope_free(Scope *s);
void scope_free_r(Scope *s);
Scope *scope_from(Scope *parent);

void scope_set_symbol(Scope *scope, StringView id, Value *value);
Value *scope_get_symbol(Scope *scope, StringView id);

typedef enum
{
    NUMBER_DECIMAL,
    NUMBER_RATIONAL,
} NumberForm;

typedef struct
{
    StringView src;
    NumberForm num_form;

    mp_prec_t prec;
    unsigned long max_digits;
    unsigned long base;

    bool use_color;
} RenderCtx;

void value_render(Value *v, String *sb, RenderCtx *ctx);

#endif
