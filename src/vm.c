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
    [VAL_EXACT]   = "exact",
    [VAL_REAL]    = "real",
    [VAL_BUILTIN] = "builtin",
    [VAL_LAMBDA]  = "lambda",
};

const char *builtin_to_str[] = {
    [BUILTIN_TRUE]  = "true",
    [BUILTIN_FALSE] = "false",
    [BUILTIN_HOLE]  = "_",
    [BUILTIN_ANS]   = "ans",
    [BUILTIN_PI]    = "pi",
    [BUILTIN_E]     = "e",
    [BUILTIN_SQRT]  = "sqrt",
};

// Free the value.
void vm_value_free(Value *v)
{
    if (!v) return;
    switch (v->kind)
    {
        case VAL_EXACT:
            mpq_clear(v->as.exact);
            break;

        case VAL_REAL:
            cr_free(v->as.real);
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

        case VAL_BUILTIN:
        case VAL_VOID:
            break;

        default:
            UNREACHABLE();
    }
    memset(v, 0, sizeof(*v));
}

// Intializes a number value.
static void value_exact(Value *v, Span span)
{
    assert(v);
    v->kind = VAL_EXACT;
    v->span = span;
    mpq_init(v->as.exact);
}

// Initializes a CReal value.
static void value_real(Value *v, Span span, CR *n)
{
    assert(v);
    v->kind = VAL_REAL;
    v->span = span;
    v->as.real = n;
}

// Initializes a bool value.
static void value_bool(Value *v, Span span, bool b)
{
    assert(v);
    v->kind = VAL_BUILTIN;
    v->span = span;
    v->as.builtin = b ? BUILTIN_TRUE : BUILTIN_FALSE;
}

static inline bool is_bool_value(Value *v)
{
    if (v->kind != VAL_BUILTIN)
        return false;
    return v->as.builtin == BUILTIN_TRUE || v->as.builtin == BUILTIN_FALSE;
}

static inline bool as_bool_value(Value *v)
{
    assert(is_bool_value(v));
    return v->as.builtin == BUILTIN_TRUE;
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

// Intializes a builtin value.
static void value_builtin(Value *v, Builtin b)
{
    assert(v);
    v->kind = VAL_BUILTIN;
    v->as.builtin = b;
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

        case VAL_EXACT:
            value_exact(v, from->span);
            mpq_set(v->as.exact, from->as.exact);
            mpq_canonicalize(v->as.exact);
            break;

        case VAL_ERROR:
            str_init_with(&v->as.error, &from->as.error);
            break;

        case VAL_REAL:
            v->as.real = cr_copy(from->as.real);
            break;

        case VAL_LAMBDA:
            Expr *l = from->as.lambda.expr;
            v->as.lambda.expr = expr_lambda(l->as.lambda.param, l->as.lambda.body);
            v->as.lambda.env = from->as.lambda.env;
            v->as.lambda.env->refcount++;
            break;

        case VAL_BUILTIN:
            v->as.builtin = from->as.builtin;
            break;

        default:
            UNREACHABLE();
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
    v->last->kind = VAL_VOID;
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
    value_exact(out, e->span);
    mpq_set(out->as.exact, e->as.number);
    return true;
}

// Evaluate a builtin identifier.
static bool eval_builtin(VM *v, Expr *e, Builtin b, Value *out)
{
    switch (b)
    {
        case BUILTIN_HOLE:
            out->kind = VAL_VOID;
            break;

        case BUILTIN_TRUE:
            value_bool(out, e->span, true);
            break;

        case BUILTIN_FALSE:
            value_bool(out, e->span, false);
            break;

        case BUILTIN_ANS:
            if (v->last)
                value_set(out, v->last);
            break;

        case BUILTIN_PI:
            value_real(out, e->span, cr_pi());
            break;

        case BUILTIN_E:
            value_real(out, e->span, cr_e());
            break;

        case BUILTIN_SQRT:
            value_builtin(out, b);
            break;

        case BUILTIN_NONE:
            break;

        default:
            UNREACHABLE();
    }
    return true;
}

// Convert an identifier to a builtin.
static Builtin id_to_builtin(Expr *e)
{
    if (e->kind != EXPR_IDENT)
        return BUILTIN_NONE;

    StringView name = SV(e->as.id);
    if (sv_equal(name, "_"))     return BUILTIN_HOLE;
    if (sv_equal(name, "true"))  return BUILTIN_TRUE;
    if (sv_equal(name, "false")) return BUILTIN_FALSE;
    if (sv_equal(name, "ans"))   return BUILTIN_ANS;
    if (sv_equal(name, "pi"))    return BUILTIN_PI;
    if (sv_equal(name, "e"))     return BUILTIN_E;
    if (sv_equal(name, "sqrt"))  return BUILTIN_SQRT;

    return BUILTIN_NONE;
}

// Evaluate an identifier
static bool eval_ident(VM *v, Expr *e, Value *out)
{
    Builtin builtin = id_to_builtin(e);
    if (builtin != BUILTIN_NONE)
        return eval_builtin(v, e, builtin, out);

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
        case VAL_EXACT:
            switch (e->as.prefix.op)
            {
                case OP_NEG:
                    mpq_neg(out->as.exact, out->as.exact);
                    return true;

                default:
                    break;
            }
            break;

        case VAL_REAL:
            switch (e->as.prefix.op)
            {
                case OP_NEG:
                    out->as.real = cr_neg(out->as.real);
                    return true;

                default:
                    break;
            }
            break;

        default:
            break;
    }
    return errorf(out, e->span, "Invalid operation: '%s' %s", 
            op_to_str[e->as.prefix.op],
            vk_to_str[out->kind]);
}

// Perform an mpq infix operation on two numbers wrapped in value.
#define MPQ_INFIX(f, l, r) f((l)->as.exact, (l)->as.exact, (r)->as.exact)

// Compare two numbers wrapped in value.
#define MPQ_CMP(l, r) mpq_cmp((l)->as.exact, (r)->as.exact)

// Evaluate one number raised to the power of the other.
static bool eval_number_power(Value *l, Value *r)
{
    if (mpz_cmp_ui(mpq_denref(r->as.exact), 1) == 0)
    {
        if (!mpz_fits_ulong_p(mpq_numref(r->as.exact)))
            return errorf(l, r->span, "Exponent too large");

        unsigned long exp = mpz_get_ui(mpq_numref(r->as.exact));
        mpz_pow_ui(mpq_numref(l->as.exact), mpq_numref(l->as.exact), exp);
        mpz_pow_ui(mpq_denref(l->as.exact), mpq_denref(l->as.exact), exp);
        mpq_canonicalize(l->as.exact);
    }
    else
    {
        return errorf(l, r->span, "Non-integer exponent is not supported");
    }
    return true;
}

// Evaluate an infix operation between two exact numbers.
static bool eval_exact_infix(Value *l, Expr *e, Value *r)
{
    Span s = e->span;
    l->span = s;

    switch (e->as.infix.op)
    {
        case OP_ADD: MPQ_INFIX(mpq_add, l, r); break;
        case OP_SUB: MPQ_INFIX(mpq_sub, l, r); break;
        case OP_MUL: MPQ_INFIX(mpq_mul, l, r); break;

        case OP_DIV:
            if (mpq_cmp_ui(r->as.exact, 0, 1) == 0)
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

// Evaluate infix between two real numbers.
static bool eval_real_infix(Value *l, Expr *e, Value *r)
{
    Span s = e->span;
    l->span = s;

    switch (e->as.infix.op)
    {
        case OP_ADD:  value_real(l, s, cr_add(l->as.real, r->as.real)); break;
        case OP_SUB:  value_real(l, s, cr_sub(l->as.real, r->as.real)); break;
        case OP_MUL:  value_real(l, s, cr_mul(l->as.real, r->as.real)); break;
        case OP_DIV:  value_real(l, s, cr_div(l->as.real, r->as.real)); break;

        default:     return errorf(l, s, "Unknown operator");
    }
    return true;
}

// Evaluate an infix operation between two booleans.
static bool eval_bool_infix(Value *l, Expr *e, Value *r)
{
    Span s = e->span;
    l->span = s;

    bool lb = as_bool_value(l);
    bool rb = as_bool_value(r);

    switch (e->as.infix.op)
    {
        case OP_EQ:  value_bool(l, s, lb == rb); break;
        case OP_NEQ: value_bool(l, s, lb != rb); break;
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

    Builtin builtin = id_to_builtin(l);
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

static bool eval_lambda_apply(VM *v, Expr *f, Value *out)
{
    if (f->as.lambda.param->kind == EXPR_IDENT)
        symbol_set(v->scope, SV(f->as.lambda.param->as.id), out);

    return vm_eval_expr(v, f->as.lambda.body, out);
}

// Convert a bool value to a church-boolean function.
static Expr *bool_as_lambda(bool v, Span span)
{
    StringView p1 = v ? SV("x") : SV("_");
    StringView p2 = v ? SV("_") : SV("y");
    StringView rt = v ? p1 : p2;

    return expr_lambda(
            expr_id(span, p1),
            expr_lambda(expr_id(span, p2), expr_id(span, rt))
        );
}

// Evaluate square root.
static bool eval_sqrt(Value *out)
{
    CR *n = NULL;

    switch (out->kind)
    {
        case VAL_REAL:
            n = cr_sqrt(out->as.real);
            break;

        case VAL_EXACT:
            CR *q = cr_from_mpq(out->as.exact);
            n = cr_sqrt(q);
            break;

        default:
            return errorf(out, out->span, "Invalid argument");
    }
    value_real(out, out->span, n);
    return true;
}

// Evaluate builtin application.
static bool eval_builtin_apply(VM *v, Value *f, Value *out)
{
    bool ok = true;

    switch (f->as.builtin)
    {
        case BUILTIN_TRUE:
        case BUILTIN_FALSE:
            Expr *fn = bool_as_lambda(f->as.builtin == BUILTIN_TRUE, f->span);
            eval_lambda_apply(v, fn, out);
            expr_destroy(&fn);
            break;

        case BUILTIN_SQRT:
            ok = eval_sqrt(out);
            break;

        case BUILTIN_HOLE:
        case BUILTIN_ANS:
            break;

        default:
            UNREACHABLE();
    }

    return ok;
}

// Evaluate an application expression.
static bool eval_apply(VM *v, Expr *f, Expr *a, Value *out)
{
    bool ok = true;

    Value func = {0};
    if (!vm_eval_expr(v, f, &func))
    {
        value_set(out, &func);
        vm_value_free(&func);
        return false;
    }

    if (!vm_eval_expr(v, a, out))
    {
        vm_value_free(&func);
        return false;
    }

    Scope *s = scope_from(func.as.lambda.env);
    Scope *prev = v->scope;
    v->scope = s;

    switch (func.kind)
    {
        case VAL_LAMBDA:
            ok = eval_lambda_apply(v, func.as.lambda.expr, out);
            break;

        case VAL_BUILTIN:
            ok = eval_builtin_apply(v, &func, out);
            break;

        default:
            ok = errorf(out, f->span, "Expected lambda");
            break;
    }

    scope_free(s);
    v->scope = prev;
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
    bool both_exact = same_kind && out->kind == VAL_EXACT;
    bool both_real = same_kind && out->kind == VAL_REAL;
    bool one_real = (out->kind == VAL_REAL && r.kind == VAL_EXACT)
        || (out->kind == VAL_EXACT && r.kind == VAL_REAL);
    bool both_bool = same_kind && is_bool_value(out);

    if (both_exact)
    {
        ok = eval_exact_infix(out, e, &r);
    }
    else if (both_real)
    {
        ok = eval_real_infix(out, e, &r);
    }
    else if (one_real)
    {
        Value *exact = out->kind == VAL_REAL ? &r : out;
        CR *n = cr_from_mpq(exact->as.exact);
        Span span = exact->span;
        vm_value_free(exact);
        value_real(exact, span, n);

        ok = eval_real_infix(out, e, &r);
    }
    else if (both_bool)
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
    if (!is_bool_value(out))
        return errorf(out, e->as.cond.if_->span, "Expected bool");

    return as_bool_value(out)
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
        value_error(out, da_last(&m));
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
    }
    module_free(&m);
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
            render_decimal(sb, v->as.exact, ctx->base, ctx->max_digits);
            break;

        case NUMBER_RATIONAL:
            char *s = mpq_get_str(NULL, ctx->base, v->as.exact);
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

// Render a builtin value.
static void value_render_builtin(Value *v, String *sb, RenderCtx *ctx)
{
    if (ctx->use_color) str_appendf(sb, ACOLOR_YELLOW);

    str_appendf(sb, "'%s", builtin_to_str[v->as.builtin]);

    if (ctx->use_color) str_appendf(sb, AFMT_RESET);
}

static bool svlist_contains(SVList *sl, StringView v)
{
    DA_FOR(sl, i)
    {
        if (sv_equal(da_at(sl, i), v))
            return true;
    }
    return false;
}

// Render an expression with identifiers substituted.
static void render_with_subst(Scope *s, Expr *e, SVList *params, String *sb, RenderCtx *ctx)
{
    Value tmp = {0};

    switch (e->kind)
    {
        case EXPR_LAMBDA:
            str_appendf(sb, "(");
            expr_render(e->as.lambda.param, sb);
            if (e->as.lambda.param->kind == EXPR_IDENT)
                da_append(params, SV(e->as.lambda.param->as.id));
            str_appendf(sb, " : ");
            render_with_subst(s, e->as.lambda.body, params, sb, ctx);
            str_appendf(sb, ")");
            break;

        case EXPR_IDENT:
            if (!svlist_contains(params, SV(e->as.id)) && symbol_get(s, SV(e->as.id), &tmp))
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
            render_with_subst(s, e->as.prefix.expr, params, sb, ctx);
            break;

        case EXPR_INFIX:
            render_with_subst(s, e->as.infix.left, params, sb, ctx);
            if (e->as.infix.op == OP_APPLY)
                str_appendf(sb, " ");
            else
                str_appendf(sb, " %s ", op_to_str[e->as.infix.op]);
            render_with_subst(s, e->as.infix.right, params, sb, ctx);
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

    SVList sl;
    da_init(&sl);

    Expr *e = v->as.lambda.expr;
    if (e->as.lambda.param->kind == EXPR_IDENT)
        da_append(&sl, SV(e->as.lambda.param->as.id));
    render_with_subst(v->as.lambda.env, e, &sl, sb, ctx);

    da_free(&sl);
    if (ctx->use_color) str_appendf(sb, AFMT_RESET);
}

static void value_render_creal(Value *v, String *sb, RenderCtx *ctx)
{
    if (ctx->use_color) str_appendf(sb, ACOLOR_YELLOW);

    mpfi_t result;
    mpfi_init2(result, ctx->prec);
    cr_eval(v->as.real, ctx->prec, result);

    render_creal(sb, result, ctx->base, ctx->max_digits);

    if (ctx->use_color) str_appendf(sb, AFMT_RESET);
}

// Render a value to the string builder.
// The render may contain newlines but will not have a final newline.
void vm_value_render(Value *v, String *sb, RenderCtx *ctx)
{
    switch (v->kind)
    {
        case VAL_ERROR:   return value_render_error(v, sb, ctx);
        case VAL_EXACT:  return value_render_number(v, sb, ctx);
        case VAL_BUILTIN: return value_render_builtin(v, sb, ctx);
        case VAL_LAMBDA:  return value_render_lambda(v, sb, ctx);
        case VAL_REAL:   return value_render_creal(v, sb, ctx);
        case VAL_VOID:    break;
        default:          UNREACHABLE();
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
