#include "value.h"

// Lookup of string names for value kind.
const char *vk_to_str[] = {
    [VAL_VOID]    = "void",
    [VAL_ERROR]   = "error",
    [VAL_EXACT]   = "exact",
    [VAL_REAL]    = "real",
    [VAL_NATIVE]  = "lambda",
    [VAL_LAMBDA]  = "lambda",
};

static Value *value_new(ValueKind kind, Span span)
{
    Value *v = malloc(sizeof(Value));
    memset(v, 0, sizeof(*v));
    v->kind = kind;
    v->span = span;
    v->refcount = 1;
    return v;
}

Value *value_void(Span span)
{
    return value_new(VAL_VOID, span);
}

Value *value_exact(Span span, const mpq_t n)
{
    Value *v = value_new(VAL_EXACT, span);
    mpq_init(v->as.exact);
    mpq_set(v->as.exact, n);
    return v;
}

Value *value_real(Span span, CR *n)
{
    Value *v = value_new(VAL_REAL, span);
    v->as.real = n;
    return v;
}

NATIVE_FN(native_bool)
{
    (void)v;
    return ud ? argv[0] : argv[1];
}

Value *value_bool(Span span, bool b)
{
    Value *out = value_native(native_bool, 2, (void *)b);
    out->span = span;
    return out;
}

bool value_is_bool(const Value *val)
{
    return val->kind == VAL_NATIVE && val->as.native.fn == native_bool;
}

bool native_to_bool(const Value *val)
{
    DEV_MUST(value_is_bool(val));
    return (bool)val->as.native.ud;
}

Value *value_lambda(const Expr *e, Scope *s)
{
    Value *v = value_new(VAL_LAMBDA, e->span);
    v->as.lambda.expr = expr_clone(e);
    v->as.lambda.env = s;
    return v;
}

Value *value_native(NativeFn fn, size_t arity, void *ud)
{
    Value *v = value_new(VAL_NATIVE, (Span){0});
    v->as.native.argc = 0;
    v->as.native.arity = arity;
    if (arity > 0)
        v->as.native.argv = malloc(arity * sizeof(Value *));
    v->as.native.fn = fn;
    v->as.native.ud = ud;
    return v;
}

Value *value_errorf(Span span, const char *fmt, ...)
{
    Value *v = value_new(VAL_ERROR, span);
    str_init(&v->as.error);

    va_list args;
    va_start(args, fmt);
    str_appendvf(&v->as.error, fmt, args);
    va_end(args);
    return v;
}

Value *value_error_from_expr(const Expr *e)
{
    DEV_MUST(expr_is_err(e));
    Value *v = value_new(VAL_ERROR, e->span);
    str_init_with(&v->as.error, &e->as.err);
    return v;
}

Value *value_error_from_cr(Span span, const CR *n)
{
    DEV_MUST(cr_is_error(n));
    Value *v = value_new(VAL_ERROR, span);
    str_init(&v->as.error);
    cr_get_error(n, &v->as.error);
    return v;
}

Value *value_error_undefined_op(const Value *l, const Expr *e, const Value *r)
{
    if (e->kind == EXPR_PREFIX)
        return value_errorf(e->span, "Undefined operation: '%s' <%s>",
                op_to_str[e->as.prefix.op], vk_to_str[r->kind]);

    return value_errorf(e->span, "Undefined operation: <%s> '%s' <%s>",
            vk_to_str[l->kind], op_to_str[e->as.infix.op], vk_to_str[r->kind]);
}

Value *value_error_expr_kind(const Expr *got, ExprKind want)
{
    return value_errorf(got->span, "Expected <%s>, got <%s>", 
            expr_to_str[want], expr_to_str[got->kind]);
}

Value *value_error_value_kind(const Value *got, ValueKind want)
{
    return value_errorf(got->span, "Expected <%s>, got <%s>", 
            vk_to_str[want], vk_to_str[got->kind]);
}

Value *value_error_value_kind_s(const Value *got, StringView want)
{
    return value_errorf(got->span, "Expected <"SV_FMT">, got <%s>", 
            SV_ARG(want), vk_to_str[got->kind]);
}

static Value *native_clone(Value *from)
{
    Value *v = value_native(from->as.native.fn, from->as.native.arity, from->as.native.ud);
    v->span = from->span;
    v->as.native.argc = from->as.native.argc;
    for (size_t i = 0; i < from->as.native.argc; i++)
        v->as.native.argv[i] = value_retain(from->as.native.argv[i]);
    return v;
}

Value *value_retain(Value *from)
{
    if (!from) return NULL;

    switch (from->kind)
    {
        case VAL_NATIVE:
            return native_clone(from);

        default:
            from->refcount++;
            return from;
    }
}

void value_release(Value **vp)
{
    if (!vp || !*vp) return;
    Value *v = *vp;
    if (v->refcount <= 0) return;
    if (--v->refcount > 0) return;

    switch (v->kind)
    {
        case VAL_EXACT:
            mpq_clear(v->as.exact);
            break;

        case VAL_REAL:
            cr_release(&v->as.real);
            break;

        case VAL_ERROR:
            str_free(&v->as.error);
            break;

        case VAL_LAMBDA:
            expr_destroy(&v->as.lambda.expr);
            scope_release(&v->as.lambda.env);
            break;

        case VAL_NATIVE:
            free(v->as.native.argv);
            break;

        case VAL_VOID:
            break;

        default:
            UNREACHABLE();
    }
    free(v);
    *vp = NULL;
}

bool value_equal(const Value *a, const Value *b)
{
    if (a->kind != b->kind) return false;

    switch (a->kind)
    {
        case VAL_VOID:
            return true;

        case VAL_ERROR:
            return sv_equal(a->as.error, b->as.error);

        case VAL_NATIVE:
            if (a->as.native.arity != b->as.native.arity)
                return false;
            if (a->as.native.argc != b->as.native.argc)
                return false;
            if (a->as.native.fn != b->as.native.fn)
                return false;
            if (a->as.native.ud != b->as.native.ud)
                return false;
            return true;

        case VAL_EXACT:
            return mpq_equal(a->as.exact, b->as.exact);

        case VAL_LAMBDA:
            return expr_equal(a->as.lambda.expr, b->as.lambda.expr);

        case VAL_REAL:
            return cr_approx(a->as.real, b->as.real);

        default:
            UNREACHABLE();
    }
}

static void symbol_free(Symbol *sym)
{
    if (!sym) return;
    if (sym->id)
    {
        str_free(sym->id);
        free(sym->id);
        sym->id = NULL;
    }
    value_release(&sym->value);
}

// Free a scope and all of its symbols.
void scope_release(Scope **sp)
{
    if (!sp || !*sp) return;

    Scope *s = *sp;
    if (s->refcount <= 0) return;
    if (--s->refcount > 0) return;

    DA_FOREACH(s, Symbol, sym) symbol_free(sym);
    da_free(s);

    if (s->parent)
        s->parent->refcount--;

    free(s);
}

// Recursively free a scope.
void scope_release_r(Scope **sp)
{
    if (!sp || !*sp) return;
    Scope *s = *sp;
    scope_release_r(&s->parent);
    scope_release(&s);
}

// Create a new scope from a parent.
Scope *scope_from(Scope *parent)
{
    Scope *s = malloc(sizeof(Scope));
    da_init(s);
    s->parent = scope_retain(parent);
    s->refcount = 1;
    return s;
}

Scope *scope_retain(Scope *s)
{
    Scope *n = s;
    while (n)
    {
        n->refcount++;
        n = n->parent;
    }
    return s;
}

// Assign a symbol to the scope.
Value *scope_set_symbol(Scope *scope, StringView id, Value *value, bool constant)
{
    if (value)
    {
        DA_FOREACH(scope, Symbol, sym)
        {
            if (sv_equal(sym->id, id))
            {
                if (sym->constant)
                    return value_errorf(value->span, "Cannot reassign constant");

                value_release(&sym->value);
                sym->value = value_retain(value);
                return NULL;
            }
        }
    }

    Symbol s = {0};
    s.id = malloc(sizeof(String));
    str_init_with(s.id, id);
    s.value = value_retain(value);
    s.constant = constant;
    da_append(scope, s);
    return NULL;
}

void scope_reset(Scope *s)
{
    DA_FOREACH(s, Symbol, sym) symbol_free(sym);
    da_reset(s);
}

// Find a symbol from the scope.
Value *scope_get_symbol(Scope *scope, StringView id)
{
    while (scope)
    {
        DA_FOREACH(scope, Symbol, sym)
        {
            if (sv_equal(sym->id, id))
                return value_retain(sym->value);
        }
        scope = scope->parent;
    }
    return NULL;
}

void matching_symbol_list(const Scope *scope, StringView name, SVList *sl)
{
    if (scope)
    {
        DA_FOREACH(scope, Symbol, s)
        {
            if (sv_startswith(SV(s->id), name))
                da_append(sl, SV(s->id));
        }
    }
}
