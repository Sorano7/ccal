#ifndef VALUE_H
#define VALUE_H

#include "cut.h"
#include "ast.h"
#include "creal.h"
#include "number.h"
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

    BUILTIN_MOD,
    BUILTIN_SQRT,

    BUILTIN_POW,
    BUILTIN_EXP,
    BUILTIN_LOG,
    BUILTIN_LN,
    _BUILTIN_COUNT,
} BuiltinKind;

extern const char *vk_to_str[];
extern const char *builtin_to_str[];

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
            size_t len;
            struct Value **args;
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
    int refcount;
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
    int refcount;
    struct Scope *parent;
} Scope;

Value *value_void(Span span);
Value *value_exact(Span span, const mpq_t n);
Value *value_real(Span span, CR *n);
Value *value_bool(Span span, bool b);
Value *value_builtin(Span span, BuiltinKind kind, size_t arity);
Value *value_lambda(const Expr *e, Scope *s);

Value *value_errorf(Span span, const char *fmt, ...);
Value *value_error_from_expr(const Expr *e);
Value *value_error_from_cr(Span span, const CR *n);
Value *value_error_undefined_op(const Value *l, const Expr *e, const Value *r);
Value *value_error_expr_kind(const Expr *got, ExprKind want);
Value *value_error_value_kind(const Value *got, ValueKind want);
Value *value_error_value_kind_s(const Value *got, StringView want);

Value *value_retain(Value *from);
void value_release(Value **vp);

#define value_is_err(v) (!v || v->kind == VAL_ERROR)

bool value_is_bool(const Value *v);
bool value_to_bool(const Value *v);

bool value_equal(const Value *a, const Value *b);

BuiltinKind builtin_kind(const Expr *e);

Scope *scope_from(Scope *parent);
Scope *scope_retain(Scope *s);
void scope_reset(Scope *s);
void scope_release(Scope **sp);
void scope_release_r(Scope **sp);

void scope_set_symbol(Scope *scope, StringView id, Value *value);
Value *scope_get_symbol(Scope *scope, StringView id);

void matching_symbol_list(const Scope *scope, StringView name, SVList *sl);

typedef struct
{
    Source *src;
    OutputFormat fmt;
    bool show_rational;

    mp_prec_t prec;
    unsigned long max_digits;
    unsigned long base;

    bool use_color;
} RenderCtx;

void render_ctx_default(RenderCtx *ctx);

void value_render(Value *v, String *sb, RenderCtx *ctx);
void scope_render(Scope *s, Value *ans, String *sb, RenderCtx *ctx);

#endif
