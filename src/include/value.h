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
    VAL_NATIVE,
} ValueKind;

extern const char *vk_to_str[];

typedef struct VM VM;
typedef struct Scope Scope;
typedef struct Value Value;

typedef Value *(*NativeFn)(VM *v, Value **argv, void *ud);

#define NATIVE_FN(name) Value *(name)(VM *v, Value **argv, void *ud)

typedef struct
{
    NativeFn fn;
    size_t arity;
    size_t argc;
    Value **argv;
    void *ud;
} Native;

typedef struct
{
    Expr *expr;
    Scope *env;
} Lambda;

// A value that an expression can evaluate to.
typedef struct Value
{
    union
    {
        String error;
        mpq_t exact;
        CR *real;
        Native native;
        Lambda lambda;
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
    bool constant;
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

NATIVE_FN(native_bool);

Value *value_void(Span span);
Value *value_exact(Span span, const mpq_t n);
Value *value_real(Span span, CR *n);
Value *value_lambda(const Expr *e, Scope *s);
Value *value_bool(Span span, bool b);
Value *value_native(NativeFn fn, size_t arity, void *ud);

bool value_is_bool(const Value *val);
bool native_to_bool(const Value *val);

#define value_is_err(v) (!v || v->kind == VAL_ERROR)
Value *value_errorf(Span span, const char *fmt, ...);
Value *value_error_from_expr(const Expr *e);
Value *value_error_from_cr(Span span, const CR *n);
Value *value_error_undefined_op(const Value *l, const Expr *e, const Value *r);
Value *value_error_expr_kind(const Expr *got, ExprKind want);
Value *value_error_value_kind(const Value *got, ValueKind want);
Value *value_error_value_kind_s(const Value *got, StringView want);

Value *value_retain(Value *from);
void value_release(Value **vp);

bool value_equal(const Value *a, const Value *b);

Scope *scope_from(Scope *parent);
Scope *scope_retain(Scope *s);
void scope_reset(Scope *s);
void scope_release(Scope **sp);
void scope_release_r(Scope **sp);

Value *scope_set_symbol(Scope *scope, StringView id, Value *value, bool constant);
Value *scope_get_symbol(Scope *scope, StringView id);

void matching_symbol_list(const Scope *scope, StringView name, SVList *sl);

#endif
