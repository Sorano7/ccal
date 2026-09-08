#include "vm.h"
#include "number.h"
#include "parser.h"

#include <assert.h>
#include <stdarg.h>

// Free a scope and all of its symbols.
static void scope_free(Scope *s)
{
    DA_FOR(s, i)
    {
        Symbol sym = da_at(s, i);
        str_free(sym.id);
        free(sym.id);
        vm_value_free(&sym.value);
    }
    da_free(s);
}

// Recursively free a scope.
static void scope_free_r(Scope *s)
{
    scope_free(s);
    while ((s = s->parent))
        scope_free(s);
}

// Create a new scope from a parent.
static Scope *scope_from(Scope *parent)
{
    Scope *s = malloc(sizeof(Scope));
    da_init(s);
    s->parent = parent;
    s->refcount = 0;
    return s;
}

// Lookup of string names for value kind.
static const char *vk_to_str[] = {
    [VAL_VOID]    = "void",
    [VAL_ERROR]   = "error",
    [VAL_NUMBER]  = "number",
    [VAL_BOOL]    = "bool",
    [VAL_LAMBDA]  = "lambda",
};

// Free the value.
void vm_value_free(Value *v)
{
    if (!v) return;
    switch (v->kind)
    {
        case VAL_NUMBER:
            mpq_clear(v->as.number);
            break;

        case VAL_ERROR:
            str_free(&v->as.error);
            break;

        case VAL_LAMBDA:
            expr_destroy(&v->as.lambda.expr);
            if (v->as.lambda.env->refcount == 0)
            {
                scope_free(v->as.lambda.env);
                free(v->as.lambda.env);
            }
            else
            {
                v->as.lambda.env->refcount--;
            }
            break;

        default:
            break;
    }
    memset(v, 0, sizeof(*v));
}

// Intializes a number value.
static void value_number(Value *v, Span span)
{
    assert(v);
    v->kind = VAL_NUMBER;
    v->span = span;
    mpq_init(v->as.number);
}

// Initializes a bool value.
static void value_bool(Value *v, Span span, bool b)
{
    assert(v);
    v->kind = VAL_BOOL;
    v->span = span;
    v->as.boolean = b;
}

// Initializes a lambda value.
static void value_lambda(Value *v, Expr *e, Scope *s)
{
    assert(v);
    assert(e->kind == EXPR_LAMBDA);
    v->kind = VAL_LAMBDA;
    v->span = e->span;
    v->as.lambda.expr = expr_clone(e);
    v->as.lambda.env = s;
}

// Initializes an error value with message.
static bool errorf(Value *v, Span span, const char *fmt, ...)
{
    v->kind = VAL_ERROR;
    v->span = span;
    str_init(&v->as.error);

    va_list args;
    va_start(args, fmt);
    str_appendvf(&v->as.error, fmt, args);
    va_end(args);
    return false;
}

// Initializes a error value from an error expression.
static bool value_error(Value *v, const Expr *e)
{
    assert(is_error(e));

    v->kind = VAL_ERROR;
    v->span = e->span;
    str_init_with(&v->as.error, &e->as.err);

    return false;
}

// Set a value from another value.
static void value_set(Value *v, const Value *from)
{
    v->kind = from->kind;
    v->span = from->span;
    switch (from->kind)
    {
        case VAL_VOID:
            v->kind = VAL_VOID;
            break;

        case VAL_NUMBER:
            value_number(v, from->span);
            mpq_set(v->as.number, from->as.number);
            mpq_canonicalize(v->as.number);
            break;

        case VAL_BOOL:
            value_bool(v, from->span, from->as.boolean);
            break;

        case VAL_ERROR:
            v->kind = VAL_ERROR;
            str_init_with(&v->as.error, &from->as.error);
            break;

        case VAL_LAMBDA:
            v->kind = VAL_LAMBDA;
            Expr *l = from->as.lambda.expr;
            v->as.lambda.expr = expr_lambda(l->as.lambda.param, l->as.lambda.body);
            v->as.lambda.env = from->as.lambda.env;
            v->as.lambda.env->refcount++;
            break;
    }
}

// Initialize a VM with default base;
void vm_init(VM *v)
{
    v->scope = scope_from(NULL);
    v->last = malloc(sizeof(Value));
    v->base = BASE_DEFAULT;
}

// Reset the state of a VM.
void vm_reset(VM *v)
{
    scope_free_r(v->scope);
    v->scope = scope_from(NULL);
    free(v->last);
    v->last = NULL;
    v->base = BASE_DEFAULT;
}

// Free a VM.
void vm_free(VM *v)
{
    scope_free_r(v->scope);
    free(v->last);
    v->last = NULL;
}

// Assign a symbol to the scope.
static void symbol_set(Scope *scope, StringView id, const Value *value)
{
    DA_FOR(scope, i)
    {
        Symbol *existing = &da_at(scope, i);
        if (sv_equal(existing->id, id))
        {
            vm_value_free(&existing->value);
            if (value)
                value_set(&existing->value, value);
            return;
        }
    }
    Symbol s = {0};
    s.id = malloc(sizeof(String));
    str_init_with(s.id, id);

    if (value)
        value_set(&s.value, value);
    da_append(scope, s);
}

// Find a symbol from the scope.
static bool symbol_get(Scope *scope, StringView id, Value *out)
{
    while (scope)
    {
        DA_FOR(scope, i)
        {
            Symbol existing = da_at(scope, i);
            if (sv_equal(existing.id, id))
            {
                if (existing.value.kind == VAL_VOID)
                    return false;
                value_set(out, &existing.value);
                return true;
            }
        }
        scope = scope->parent;
    }
    return false;
}

// Evaluate a number expression.
static bool eval_number(Expr *e, Value *out)
{
    value_number(out, e->span);
    mpq_set(out->as.number, e->as.number);
    return true;
}

#define ID_PREFIX "'"

// A function to evaluate a builtin identifier.
typedef bool (*BuiltinIdFn)(VM *, Expr *, Value *);

static bool eval_builtin_true(VM *v, Expr *e, Value *out)
{
    (void)v;
    value_bool(out, e->span, true);
    return true;
}

static bool eval_builtin_false(VM *v, Expr *e, Value *out)
{
    (void)v;
    value_bool(out, e->span, false);
    return true;
}

static bool eval_builtin_ans(VM *v, Expr *e, Value *out)
{
    (void)e;
    if (v->last)
        value_set(out, v->last);
    return true;
}

static bool eval_builtin_hole(VM *v, Expr *e, Value *out)
{
    (void)v; (void)e;
    out->kind = VAL_VOID;
    return true;
}

// List of builtin identifiers.
typedef enum
{
    BUILTIN_NONE,
    BUILTIN_HOLE,
    BUILTIN_TRUE,
    BUILTIN_FALSE,
    BUILTIN_ANS,
} BuiltinId;

// Lookup for evaluation functions for builtin identifiers.
static const BuiltinIdFn builtin_id_eval[] = {
    [BUILTIN_NONE]  = NULL,
    [BUILTIN_HOLE]  = eval_builtin_hole,
    [BUILTIN_TRUE]  = eval_builtin_true,
    [BUILTIN_FALSE] = eval_builtin_false,
    [BUILTIN_ANS]   = eval_builtin_ans,
};

// Convert an identifier to a builtin.
static BuiltinId id_to_builtin(Expr *e)
{
    if (e->kind != EXPR_IDENT || e->as.id.len <= 1)
        return BUILTIN_NONE;

    StringView name = sv_slice(SV(e->as.id),.from=1);
    if (sv_equal(name, "_"))     return BUILTIN_HOLE;
    if (sv_equal(name, "true"))  return BUILTIN_TRUE;
    if (sv_equal(name, "false")) return BUILTIN_FALSE;
    if (sv_equal(name, "ans"))   return BUILTIN_ANS;

    return BUILTIN_NONE;
}

// Evaluate an identifier
static bool eval_ident(VM *v, Expr *e, Value *out)
{
    if (e->as.id.len <= 1)
        return errorf(out, e->span, "Empty identifier");

    BuiltinId builtin = id_to_builtin(e);
    if (builtin != BUILTIN_NONE)
        return builtin_id_eval[builtin](v, e, out);

    if (!symbol_get(v->scope, SV(e->as.id), out))
        return errorf(out, e->span, "undefined symbol");
    return true;
}

// Evaluate a prefix expression.
static bool eval_prefix(VM *v, Expr *e, Value *out)
{
    if (!vm_eval_expr(v, e->as.prefix.expr, out))
        return false;

    switch (out->kind)
    {
        case VAL_NUMBER:
            switch (e->as.prefix.op)
            {
                case OP_NEG:
                    mpq_neg(out->as.number, out->as.number);
                    return true;

                default:
                    // fallthrough
            }
            // fallthrough
        default:
            return errorf(out, e->span, "Invalid operation: '%s' %s", 
                    op_to_str[e->as.prefix.op],
                    vk_to_str[out->kind]);
    }
}

// Perform an mpq infix operation on two numbers wrapped in value.
#define MPQ_INFIX(f, l, r) f((l)->as.number, (l)->as.number, (r)->as.number)

// Compare two numbers wrapped in value.
#define MPQ_CMP(l, r) mpq_cmp((l)->as.number, (r)->as.number)

// Evaluate one number raised to the power of the other.
static bool eval_number_power(Value *l, Value *r)
{
    if (mpz_cmp_ui(mpq_denref(r->as.number), 1) == 0)
    {
        if (!mpz_fits_ulong_p(mpq_numref(r->as.number)))
            return errorf(l, r->span, "Exponent too large");

        unsigned long exp = mpz_get_ui(mpq_numref(r->as.number));
        mpz_pow_ui(mpq_numref(l->as.number), mpq_numref(l->as.number), exp);
        mpz_pow_ui(mpq_denref(l->as.number), mpq_denref(l->as.number), exp);
        mpq_canonicalize(l->as.number);
    }
    else
    {
        return errorf(l, r->span, "Non-integer exponent is not supported");
    }
    return true;
}

// Evaluate an infix operation between two numbers.
static bool eval_number_infix(Value *l, Expr *e, Value *r)
{
    Span s = e->span;
    l->span = s;

    switch (e->as.infix.op)
    {
        case OP_ADD: MPQ_INFIX(mpq_add, l, r); break;
        case OP_SUB: MPQ_INFIX(mpq_sub, l, r); break;
        case OP_MUL: MPQ_INFIX(mpq_mul, l, r); break;

        case OP_DIV:
            if (mpq_cmp_ui(r->as.number, 0, 1) == 0)
                return errorf(l, r->span, "Division by zero");
            MPQ_INFIX(mpq_div, l, r);
            break;

        case OP_POW: return eval_number_power(l, r);

        case OP_LT:  value_bool(l, s, MPQ_CMP(l, r) < 0);  break;
        case OP_LEQ: value_bool(l, s, MPQ_CMP(l, r) <= 0); break;
        case OP_GT:  value_bool(l, s, MPQ_CMP(l, r) > 0);  break;
        case OP_GEQ: value_bool(l, s, MPQ_CMP(l, r) >= 0); break;
        case OP_EQ:  value_bool(l, s, MPQ_CMP(l, r) == 0); break;
        case OP_NEQ: value_bool(l, s, MPQ_CMP(l, r) != 0); break;

        default:     return errorf(l, s, "Unknown operator");
    }
    return true;
}

// Evaluate an infix operation between two booleans.
static bool eval_bool_infix(Value *l, Expr *e, Value *r)
{
    Span s = e->span;
    l->span = s;

    switch (e->as.infix.op)
    {
        case OP_EQ:  value_bool(l, s, l->as.boolean == r->as.boolean); break;
        case OP_NEQ: value_bool(l, s, l->as.boolean != r->as.boolean); break;
        default:     return errorf(l, s, "Unknown operator");
    }
    return true;
}

// Evaluate an assignment infix operation.
static bool eval_assign_infix(VM *v, Expr *e, Value *out)
{
    Expr *l = e->as.infix.left;

    if (l->kind != EXPR_IDENT)
        return errorf(out, l->span, "Expected identifier");

    BuiltinId builtin = id_to_builtin(l);
    if (builtin != BUILTIN_NONE && builtin != BUILTIN_HOLE)
        return errorf(out, l->span, "Cannot assign to builtin identifier");

    if (!vm_eval_expr(v, e->as.infix.right, out))
        return false;

    if (builtin != BUILTIN_HOLE)
    {
        if (out->kind == VAL_LAMBDA)
            symbol_set(out->as.lambda.env, SV(l->as.id), out);

        symbol_set(v->scope, SV(l->as.id), out);
    }
    return true;
}

// Convert a bool value to a church-boolean function.
static Expr *bool_as_lambda(Value *b)
{
    StringView p1 = b->as.boolean ? SV(ID_PREFIX"x") : SV(ID_PREFIX"_");
    StringView p2 = b->as.boolean ? SV(ID_PREFIX"_") : SV(ID_PREFIX"y");
    StringView rt = b->as.boolean ? p1 : p2;

    return expr_lambda(
            expr_id(b->span, p1),
            expr_lambda(expr_id(b->span, p2), expr_id(b->span, rt))
        );
}

// Evaluate an application expression.
static bool eval_apply(VM *v, Expr *f, Expr *a, Value *out)
{
    bool ok = true;

    Value func = {0};
    if (!vm_eval_expr(v, f, &func))
    {
        value_set(out, &func);
        ok = false;
        goto cleanup;
    }

    if (func.kind != VAL_LAMBDA && func.kind != VAL_BOOL)
    {
        ok = errorf(out, f->span, "Expected lambda");
        goto cleanup;
    }
    if (!vm_eval_expr(v, a, out))
    {
        ok = false;
        goto cleanup;
    }

    Expr *lam = func.kind == VAL_LAMBDA
        ? func.as.lambda.expr
        : bool_as_lambda(&func);

    Scope *s = scope_from(func.as.lambda.env);

    Scope *prev = v->scope;
    v->scope = s;

    if (lam->as.lambda.param->kind == EXPR_IDENT)
        symbol_set(s, SV(lam->as.lambda.param->as.id), out);

    ok = vm_eval_expr(v, lam->as.lambda.body, out);

    scope_free(s);
    v->scope = prev;

    if (func.kind == VAL_BOOL)
        expr_destroy(&lam);

cleanup:
    vm_value_free(&func);
    return ok;
}

// Evaluate an infix operation.
static bool eval_infix(VM *v, Expr *e, Value *out)
{
    if (e->as.infix.op == OP_ASSIGN)
        return eval_assign_infix(v, e, out);

    if (e->as.infix.op == OP_APPLY || e->as.infix.op == OP_PIPE)
        return eval_apply(v, e->as.infix.left, e->as.infix.right, out);

    bool ok = false;

    if (!vm_eval_expr(v, e->as.infix.left, out))
        return false;

    Value r = {0};
    if (!vm_eval_expr(v, e->as.infix.right, &r))
    {
        value_set(out, &r);
        goto cleanup;
    }

    bool same_kind = out->kind == r.kind;

    if (same_kind && out->kind == VAL_NUMBER)
    {
        ok = eval_number_infix(out, e, &r);
    }
    else if (same_kind && out->kind == VAL_BOOL)
    {
        ok = eval_bool_infix(out, e, &r);
    }
    else
    {
        ok = errorf(out, e->span,
                "Invalid operation: %s '%s' %s",
                vk_to_str[out->kind], op_to_str[e->as.infix.op], vk_to_str[r.kind]);
    }

cleanup:
    vm_value_free(&r);
    return ok;
}

// Capture free variables from the current scope that are no shadowed by params.
static void capture_free_vars(VM *v, Expr *e, Scope *s)
{
    Value tmp = {0};

    switch (e->kind)
    {
        case EXPR_IDENT:
            if (symbol_get(v->scope, SV(e->as.id), &tmp))
                symbol_set(s, SV(e->as.id), &tmp);
            break;

        case EXPR_INFIX:
            capture_free_vars(v, e->as.infix.left, s);
            capture_free_vars(v, e->as.infix.right, s);
            break;

        case EXPR_PREFIX:
            capture_free_vars(v, e->as.prefix.expr, s);
            break;

        case EXPR_LAMBDA:
            capture_free_vars(v, e->as.lambda.body, s);
            break;

        case EXPR_COND:
            capture_free_vars(v, e->as.cond.if_, s);
            capture_free_vars(v, e->as.cond.then, s);
            capture_free_vars(v, e->as.cond.else_, s);
            break;

        case EXPR_ERROR:
        case EXPR_NUMBER:
            break;

        default:
            UNREACHABLE();
    }
    vm_value_free(&tmp);
}

// Evaluate a lambda expression.
static bool eval_lambda(VM *v, Expr *e, Value *out)
{
    Scope *s = scope_from(NULL);
    capture_free_vars(v, e->as.lambda.body, s);

    value_lambda(out, e, s);
    return true;
}

// Evaluate a conditional expression.
static bool eval_cond(VM *v, Expr *e, Value *out)
{
    if (!vm_eval_expr(v, e->as.cond.if_, out))
        return false;
    if (out->kind != VAL_BOOL)
        return errorf(out, e->as.cond.if_->span, "Expected bool");

    return out->as.boolean 
        ? vm_eval_expr(v, e->as.cond.then, out)
        : vm_eval_expr(v, e->as.cond.else_, out);
}

// Evaluate an expression.
bool vm_eval_expr(VM *v, Expr *e, Value *out)
{
    bool ok = true;

    assert(v && e && out);
    out->kind = VAL_VOID;

    if (is_error(e))
        return value_error(out, e);

    switch (e->kind)
    {
        case EXPR_NUMBER: ok = eval_number(e, out);    break;
        case EXPR_IDENT:  ok = eval_ident(v, e, out);  break;
        case EXPR_INFIX:  ok = eval_infix(v, e, out);  break;
        case EXPR_PREFIX: ok = eval_prefix(v, e, out); break;
        case EXPR_LAMBDA: ok = eval_lambda(v, e, out); break;
        case EXPR_COND:   ok = eval_cond(v, e, out);   break;
        default:          UNREACHABLE();
    }

    if (ok)
        value_set(v->last, out);
    return ok;
}

// Run and evaluate a source.
bool vm_run(VM *v, StringView src, Value *out)
{
    Module m;
    da_init(&m);

    bool ok = parse_module(src, v->base, &m);
    if (!ok)
    {
        value_error(out, m.data[0]);
        module_free(&m);
    }

    DA_FOR(&m, i)
    {
        Expr *e = da_at(&m, i);
        if (!vm_eval_expr(v, e, out))
        {
            module_free(&m);
            return false;
        }
        expr_destroy(&e);
    }

    return ok;
}

// Render a number value.
static void value_render_number(Value *v, String *sb, RenderCtx *ctx)
{
    if (ctx->base >= 62)
    {
        if (ctx->use_color) str_appendf(sb, ACOLOR_MAGENTA);
        str_append(sb, "Output base too large");
        if (ctx->use_color) str_appendf(sb, AFMT_RESET);
        return;
    }

    if (ctx->base != BASE_DEFAULT)
    {
        if (ctx->use_color) str_appendf(sb, AFMT_DIM);
        str_appendf(sb, "%lu#", ctx->base);
        if (ctx->use_color) str_appendf(sb, AFMT_RESET);
    }

    if (ctx->use_color) str_appendf(sb, ACOLOR_YELLOW);
    switch (ctx->num_form)
    {
        case NUMBER_DECIMAL:
            render_decimal(sb, v->as.number, ctx->base, ctx->max_digits);
            break;

        case NUMBER_RATIONAL:
            char *s = mpq_get_str(NULL, ctx->base, v->as.number);
            str_append(sb, s);
            free(s);
            break;
    }

    if (ctx->use_color) str_appendf(sb, AFMT_RESET);
}

// Render an error value.
static void value_render_error(Value *v, String *sb, RenderCtx *ctx)
{
    str_appendf(sb, SV_FMT"\n", SV_ARG(ctx->src));
    for (size_t i = 0; i < v->span.from; i++)
        str_append(sb, " ");

    if (ctx->use_color) str_appendf(sb, AFMT_BOLD ACOLOR_MAGENTA);
    for (size_t i = 0; i < v->span.to - v->span.from; i++)
        str_appendf(sb, "^");

    str_appendf(sb, " "SV_FMT, SV_ARG(SV(v->as.error)));
    if (ctx->use_color) str_appendf(sb, AFMT_RESET);
}

// Render a bool value.
static void value_render_bool(Value *v, String *sb, RenderCtx *ctx)
{
    if (ctx->use_color) str_appendf(sb, ACOLOR_YELLOW);
    str_appendf(sb, ID_PREFIX"%s", v->as.boolean ? "true" : "false");
    if (ctx->use_color) str_appendf(sb, AFMT_RESET);
}

// Render an expression with identifiers substituted.
static void render_with_subst(Scope *s, Expr *e, String *sb, RenderCtx *ctx)
{
    Value tmp = {0};

    switch (e->kind)
    {
        case EXPR_LAMBDA:
            expr_render(e->as.lambda.param, sb);
            str_appendf(sb, " : ");
            render_with_subst(s, e->as.lambda.body, sb, ctx);
            break;

        case EXPR_IDENT:
            if (symbol_get(s, SV(e->as.id), &tmp))
            {
                if (tmp.kind != VAL_LAMBDA || s != tmp.as.lambda.env)
                {
                    vm_value_render(&tmp, sb, ctx);
                    if (ctx->use_color) str_appendf(sb, ACOLOR_YELLOW);
                    break;
                }
            }
                expr_render(e, sb);
            break;

        case EXPR_PREFIX:
            str_appendf(sb, " %s", op_to_str[e->as.prefix.op]);
            render_with_subst(s, e->as.prefix.expr, sb, ctx);
            break;

        case EXPR_INFIX:
            render_with_subst(s, e->as.infix.left, sb, ctx);
            if (e->as.infix.op == OP_APPLY)
                str_appendf(sb, " ");
            else
                str_appendf(sb, " %s ", op_to_str[e->as.infix.op]);
            render_with_subst(s, e->as.infix.right, sb, ctx);
            break;

        default:
            expr_render(e, sb);
            break;
    }
    vm_value_free(&tmp);
}

// Render a lambda value;
static void value_render_lambda(Value *v, String *sb, RenderCtx *ctx)
{
    if (ctx->use_color) str_appendf(sb, ACOLOR_YELLOW);

    Expr *e = v->as.lambda.expr;
    render_with_subst(v->as.lambda.env, e, sb, ctx);

    if (ctx->use_color) str_appendf(sb, AFMT_RESET);
}

// Render a value to the string builder.
// The render may contain newlines but will not have a final newline.
void vm_value_render(Value *v, String *sb, RenderCtx *ctx)
{
    switch (v->kind)
    {
        case VAL_ERROR:  return value_render_error(v, sb, ctx);
        case VAL_NUMBER: return value_render_number(v, sb, ctx);
        case VAL_BOOL:   return value_render_bool(v, sb, ctx);
        case VAL_LAMBDA: return value_render_lambda(v, sb, ctx);
        case VAL_VOID:   break;
        default:         UNREACHABLE();
    }
}

// Render the current environment of the VM.
void vm_env_render(VM *v, String *sb, RenderCtx *ctx)
{
    Scope *scope = v->scope;
    if (ctx->use_color) str_appendf(sb, ACOLOR_CYAN);
    str_appendf(sb, "Env (%zu)\n", scope->len);
    if (ctx->use_color) str_appendf(sb, AFMT_RESET);

    DA_FOR(v->scope, i)
    {
        Symbol sym = da_at(v->scope, i);
        str_appendf(sb, "    "SV_FMT" = ", SV_ARG(SV(sym.id)));
        vm_value_render(&sym.value, sb, ctx);
        str_append(sb, "\n");
    }
    if (ctx->use_color) str_appendf(sb, AFMT_RESET);
}
