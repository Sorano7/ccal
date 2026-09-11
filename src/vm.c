#include "vm.h"
#include "number.h"
#include "parser.h"
#include <stdarg.h>

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

// Evaluate a number expression.
static bool eval_number(Expr *e, Value *out)
{
    value_exact(out, e->span);
    mpq_set(out->as.exact, e->as.number);
    return true;
}

// Evaluate a builtin identifier.
static bool eval_builtin(VM *v, Expr *e, BuiltinKind b, Value *out)
{
    switch (b)
    {
        case BUILTIN_HOLE:
            out->kind = VAL_VOID;
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
        case BUILTIN_EXP:
        case BUILTIN_LN:
            value_builtin(out, e->span, b, 1);
            break;

        case BUILTIN_TRUE:
        case BUILTIN_FALSE:
        case BUILTIN_POW:
        case BUILTIN_LOG:
            value_builtin(out, e->span, b, 2);
            break;

        case BUILTIN_NONE:
            break;

        default:
            UNREACHABLE();
    }
    return true;
}

// Evaluate an identifier
static bool eval_ident(VM *v, Expr *e, Value *out)
{
    BuiltinKind builtin = builtin_kind(e);
    if (builtin != BUILTIN_NONE)
        return eval_builtin(v, e, builtin, out);

    if (!symbol_get(v->scope, SV(e->as.id), out))
        return value_errorf(out, e->span, "undefined symbol");
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
    return value_errorf(out, e->span, "Invalid operation: '%s' %s", 
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
            return value_errorf(l, r->span, "Exponent too large");

        unsigned long exp = mpz_get_ui(mpq_numref(r->as.exact));
        mpz_pow_ui(mpq_numref(l->as.exact), mpq_numref(l->as.exact), exp);
        mpz_pow_ui(mpq_denref(l->as.exact), mpq_denref(l->as.exact), exp);
        mpq_canonicalize(l->as.exact);
    }
    else
    {
        return value_errorf(l, r->span, "Non-integer exponent is not supported");
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
                return value_errorf(l, r->span, "Division by zero");
            MPQ_INFIX(mpq_div, l, r);
            break;

        case OP_POW: return eval_number_power(l, r);

        case OP_LT:  value_bool(l, s, MPQ_CMP(l, r) < 0);  break;
        case OP_LEQ: value_bool(l, s, MPQ_CMP(l, r) <= 0); break;
        case OP_GT:  value_bool(l, s, MPQ_CMP(l, r) > 0);  break;
        case OP_GEQ: value_bool(l, s, MPQ_CMP(l, r) >= 0); break;
        case OP_EQ:  value_bool(l, s, MPQ_CMP(l, r) == 0); break;
        case OP_NEQ: value_bool(l, s, MPQ_CMP(l, r) != 0); break;

        default:     return value_errorf(l, s, "Unknown operator");
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

        default:     return value_errorf(l, s, "Unknown operator");
    }
    return true;
}

// Evaluate an infix operation between two booleans.
static bool eval_bool_infix(Value *l, Expr *e, Value *r)
{
    Span s = e->span;
    l->span = s;

    bool lb = value_to_bool(l);
    bool rb = value_to_bool(r);

    switch (e->as.infix.op)
    {
        case OP_EQ:  value_bool(l, s, lb == rb); break;
        case OP_NEQ: value_bool(l, s, lb != rb); break;
        default:     return value_errorf(l, s, "Unknown operator");
    }
    return true;
}

// Evaluate an assignment infix operation.
static bool eval_assign_infix(VM *v, Expr *e, Value *out)
{
    Expr *l = e->as.infix.left;

    if (l->kind != EXPR_IDENT)
        return value_errorf(out, l->span, "Expected identifier");

    BuiltinKind builtin = builtin_kind(l);
    if (builtin != BUILTIN_NONE && builtin != BUILTIN_HOLE)
        return value_errorf(out, l->span, "Cannot assign to builtin identifier");

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

// Evaluate a lambda application.
static bool eval_lambda_apply(VM *v, Expr *f, Value *out)
{
    if (f->as.lambda.param->kind == EXPR_IDENT)
        symbol_set(v->scope, SV(f->as.lambda.param->as.id), out);
    return vm_eval_expr(v, f->as.lambda.body, out);
}

// Evaluate a builtin bool application.
static bool eval_builtin_bool(Value *f, Value *out)
{
    if (!value_is_bool(f)) return true;
    // value_free(out);
    value_set(out, &da_at(&f->as.builtin.args, 0));
    return true;
}

// Evaluate a builtin unary function on real values.
static bool eval_builtin_real_unary(Value *out, CRUnary fn)
{
    CR *x = NULL;
    switch (out->kind)
    {
        case VAL_REAL : x = out->as.real;               break;
        case VAL_EXACT: x = cr_from_mpq(out->as.exact); break;
        default:        return value_errorf(out, out->span, "Invalid argument");
    }
    value_free(out);
    value_real(out, out->span, fn(x));
    cr_release(x);
    return true;
}

// Evaluate builtin application.
static bool eval_builtin_apply(VM *v, Value *f, Value *out)
{
    bool ok = true;

    if (f->as.builtin.args.len + 1 < f->as.builtin.arity)
    {
        da_grow(&f->as.builtin.args);
        Value *ptr = &f->as.builtin.args.data[f->as.builtin.args.len++];
        value_set(ptr, out);
        // value_free(out);
        value_set(out, f);
        return true;
    }

    switch (f->as.builtin.kind)
    {
        case BUILTIN_TRUE:
        case BUILTIN_FALSE: ok = eval_builtin_bool(f, out);             break;
        case BUILTIN_SQRT:  ok = eval_builtin_real_unary(out, cr_sqrt); break;
        case BUILTIN_EXP:   ok = eval_builtin_real_unary(out, cr_exp);  break;
        case BUILTIN_LN:    ok = eval_builtin_real_unary(out, cr_ln);   break;

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
        value_free(&func);
        return false;
    }

    if (!vm_eval_expr(v, a, out))
    {
        value_free(&func);
        return false;
    }

    Scope *s = scope_from(func.as.lambda.env);
    Scope *prev = v->scope;
    v->scope = s;

    switch (func.kind)
    {
        case VAL_LAMBDA:  ok = eval_lambda_apply(v, func.as.lambda.expr, out); break;
        case VAL_BUILTIN: ok = eval_builtin_apply(v, &func, out);              break;
        default:
              ok = value_errorf(out, f->span, 
                      "Invalid application to %s", vk_to_str[func.kind]);
              break;
    }

    scope_free(s);
    v->scope = prev;
    value_free(&func);
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

    bool same_kind  = out->kind == r.kind;
    bool both_exact = same_kind && out->kind == VAL_EXACT;
    bool both_real  = same_kind && out->kind == VAL_REAL;
    bool one_real   = (out->kind == VAL_REAL && r.kind == VAL_EXACT)
                        || (out->kind == VAL_EXACT && r.kind == VAL_REAL);
    bool both_bool  = same_kind && value_is_bool(out);

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
        value_free(exact);
        value_real(exact, span, n);

        ok = eval_real_infix(out, e, &r);
    }
    else if (both_bool)
    {
        ok = eval_bool_infix(out, e, &r);
    }
    else
    {
        ok = value_errorf(out, e->span,
                "Invalid operation: %s '%s' %s",
                vk_to_str[out->kind], op_to_str[e->as.infix.op], vk_to_str[r.kind]);
    }

cleanup:
    value_free(&r);
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
    value_free(&tmp);
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
    if (!value_is_bool(out))
        return value_errorf(out, e->as.cond.if_->span, "Expected bool");

    return value_to_bool(out)
        ? vm_eval_expr(v, e->as.cond.then, out)
        : vm_eval_expr(v, e->as.cond.else_, out);
}

// Evaluate an expression.
bool vm_eval_expr(VM *v, Expr *e, Value *out)
{
    bool ok = true;

    DEV_MUST(v && e && out);
    out->kind = VAL_VOID;

    if (is_error(e))
        return value_error_from_expr(out, e);

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
        value_error_from_expr(out, da_last(&m));
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
        value_render(&sym.value, sb, ctx);
        str_append(sb, "\n");
    }
    if (ctx->use_color) str_appendf(sb, AFMT_RESET);
}
