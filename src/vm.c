#include "vm.h"
#include "number.h"
#include "parser.h"
#include <stdarg.h>

// Initialize a VM with default base;
void vm_init(VM *v)
{
    v->scope = scope_from(NULL);
    v->last = NULL;
    v->base = BASE_DEFAULT;
}

// Reset the state of a VM.
void vm_reset(VM *v)
{
    scope_free_r(v->scope);
    v->scope = scope_from(NULL);
    if (v->last) value_release(v->last);
    v->last = NULL;
    v->base = BASE_DEFAULT;
}

// Free a VM.
void vm_free(VM *v)
{
    scope_free_r(v->scope);
    if (v->last) value_release(v->last);
}

// Evaluate a number expression.
static Value *eval_number(Expr *e)
{
    return value_exact(e->span, e->as.number);
}

// Evaluate a builtin identifier.
static Value *eval_builtin(VM *v, Expr *e, BuiltinKind b)
{
    switch (b)
    {
        case BUILTIN_ANS:  if (v->last) return value_retain(v->last); break;
        case BUILTIN_PI:   return value_real(e->span, cr_pi());       break;
        case BUILTIN_E:    return value_real(e->span, cr_e());        break;

        case BUILTIN_SQRT:
        case BUILTIN_EXP:
        case BUILTIN_LN:   return value_builtin(e->span, b, 1); break;

        case BUILTIN_TRUE:
        case BUILTIN_FALSE:
        case BUILTIN_POW:
        case BUILTIN_LOG:  return value_builtin(e->span, b, 2); break;

        case BUILTIN_HOLE: break;
        case BUILTIN_NONE:
        default:           UNREACHABLE();
    }
    return value_void(e->span);
}

// Evaluate an identifier
static Value *eval_ident(VM *v, Expr *e)
{
    BuiltinKind builtin = builtin_kind(e);
    if (builtin != BUILTIN_NONE)
        return eval_builtin(v, e, builtin);

    Value *out = scope_get_symbol(v->scope, SV(e->as.id));
    return out ? out : value_errorf(e->span, "undefined symbol");
}

// Evaluate a prefix expression.
static Value *eval_prefix(VM *v, Expr *e)
{
    Value *out = vm_eval_expr(v, e->as.prefix.expr);
    if (value_is_err(out)) return out;

    switch (out->kind)
    {
        case VAL_EXACT:
            switch (e->as.prefix.op)
            {
                case OP_NEG:
                    mpq_neg(out->as.exact, out->as.exact);
                    return out;

                default:
                    break;
            }
            break;

        case VAL_REAL:
            switch (e->as.prefix.op)
            {
                case OP_NEG:
                    out->as.real = cr_neg(out->as.real);
                    return out;

                default:
                    break;
            }
            break;

        default:
            break;
    }

    value_release(out);
    return value_errorf(e->span, "Invalid operation: '%s' %s", 
            op_to_str[e->as.prefix.op],
            vk_to_str[out->kind]);
}

// Perform an mpq infix operation on two numbers wrapped in value.
#define MPQ_INFIX(f, out, l, r) f((out)->as.exact, (l)->as.exact, (r)->as.exact)

// Compare two numbers wrapped in value.
#define MPQ_CMP(l, r) mpq_cmp((l)->as.exact, (r)->as.exact)

// Evaluate one number raised to the power of the other.
static Value *eval_number_power(Value *l, Value *r)
{
    if (mpz_cmp_ui(mpq_denref(r->as.exact), 1) != 0)
        return value_errorf(r->span, "Non-integer exponent is not supported");

    if (!mpz_fits_ulong_p(mpq_numref(r->as.exact)))
        return value_errorf(r->span, "Exponent too large");

    Span s = {l->span.from, r->span.to};
    Value *out = value_exact(s, l->as.exact);

    unsigned long exp = mpz_get_ui(mpq_numref(r->as.exact));
    mpz_pow_ui(mpq_numref(out->as.exact), mpq_numref(l->as.exact), exp);
    mpz_pow_ui(mpq_denref(out->as.exact), mpq_denref(l->as.exact), exp);
    mpq_canonicalize(out->as.exact);
    return out;
}

// Evaluate comparison and equality between exact values.
static Value *eval_exact_bool_infix(Value *l, Expr *e, Value *r)
{
    bool b;

    switch (e->as.infix.op)
    {
        case OP_LT:  b = MPQ_CMP(l, r) < 0;  break;
        case OP_LEQ: b = MPQ_CMP(l, r) <= 0; break;
        case OP_GT:  b = MPQ_CMP(l, r) > 0;  break;
        case OP_GEQ: b = MPQ_CMP(l, r) >= 0; break;
        case OP_EQ:  b = MPQ_CMP(l, r) == 0; break;
        case OP_NEQ: b = MPQ_CMP(l, r) != 0; break;
        default:     UNREACHABLE();
    }
    return value_bool(e->span, b);
}

// Check is an operator is comparison or equality.
static bool op_is_cmp_or_eq(Operator op)
{
    switch (op)
    {
        case OP_LT:
        case OP_LEQ:
        case OP_GT:
        case OP_GEQ:
        case OP_EQ:
        case OP_NEQ:
            return true;
        default:
            return false;
    }
}

// Evaluate an infix operation between two exact numbers.
static Value *eval_exact_infix(Value *l, Expr *e, Value *r)
{
    if (e->as.infix.op == OP_POW)
        return eval_number_power(l, r);
    if (op_is_cmp_or_eq(e->as.infix.op))
        return eval_exact_bool_infix(l, e, r);

    Value *out = value_exact(e->span, l->as.exact);
    switch (e->as.infix.op)
    {
        case OP_ADD: MPQ_INFIX(mpq_add, out, l, r); break;
        case OP_SUB: MPQ_INFIX(mpq_sub, out, l, r); break;
        case OP_MUL: MPQ_INFIX(mpq_mul, out, l, r); break;

        case OP_DIV:
            if (mpq_cmp_ui(r->as.exact, 0, 1) == 0)
            {
                value_release(out);
                return value_errorf(r->span, "Division by zero");
            }
            MPQ_INFIX(mpq_div, out, l, r);
            break;

        default:
            value_release(out);
            return value_errorf(e->span, "Unknown operator");
    }
    return out;
}

// Evaluate infix between two real numbers.
static Value *eval_real_infix(Value *l, Expr *e, Value *r)
{
    CR *n = NULL;
    switch (e->as.infix.op)
    {
        case OP_ADD: n = cr_add(l->as.real, r->as.real); break;
        case OP_SUB: n = cr_sub(l->as.real, r->as.real); break;
        case OP_MUL: n = cr_mul(l->as.real, r->as.real); break;
        case OP_DIV: n = cr_div(l->as.real, r->as.real); break;
        default:     return value_errorf(e->span, "Unknown operator");
    }
    return value_real(e->span, n);
}

// Evaluate an infix operation between two booleans.
static Value *eval_bool_infix(Value *l, Expr *e, Value *r)
{
    bool lb = value_to_bool(l);
    bool rb = value_to_bool(r);
    bool vb = false;

    switch (e->as.infix.op)
    {
        case OP_EQ:  vb = lb == rb; break;
        case OP_NEQ: vb = lb != rb; break;
        default:     return value_errorf(e->span, "Unknown operator");
    }
    return value_bool(e->span, vb);
}

// Evaluate an assignment infix operation.
static Value *eval_assign_infix(VM *v, Expr *e)
{
    Expr *l = e->as.infix.left;
    if (l->kind != EXPR_IDENT)
        return value_errorf(l->span, "Expected identifier");

    BuiltinKind builtin = builtin_kind(l);
    if (builtin != BUILTIN_NONE && builtin != BUILTIN_HOLE)
        return value_errorf(l->span, "Cannot assign to builtin identifier");

    Value *out = vm_eval_expr(v, e->as.infix.right);
    if (value_is_err(out)) return out;

    if (builtin != BUILTIN_HOLE)
    {
        if (out->kind == VAL_LAMBDA)
            scope_set_symbol(out->as.lambda.env, SV(l->as.id), out);
        scope_set_symbol(v->scope, SV(l->as.id), out);
    }
    return out;
}

#define AS_REAL(val, cr) do { \
    switch ((val)->kind) { \
        case VAL_REAL:  (cr) = (val)->as.real;               break; \
        case VAL_EXACT: (cr) = cr_from_mpq((val)->as.exact); break; \
        default:        return value_errorf((val)->span, "Invalid argument"); \
    } \
} while (0)

// Evaluate a lambda application.
static Value *eval_lambda_apply(VM *v, Value *f, Value *arg)
{
    Scope *s = scope_from(f->as.lambda.env);
    Scope *prev = v->scope;
    v->scope = s;

    Expr *func = f->as.lambda.expr;
    if (func->as.lambda.param->kind == EXPR_IDENT)
        scope_set_symbol(v->scope, SV(func->as.lambda.param->as.id), arg);
    Value *out = vm_eval_expr(v, func->as.lambda.body);

    v->scope = prev;
    scope_free(s);
    return out;
}

// Evaluate a builtin bool application.
static Value *eval_builtin_bool(Value *f, Value *arg)
{
    if (!value_to_bool(f)) return value_retain(arg);
    return value_retain(da_at(&f->as.builtin.args, 0));
}

// Evaluate a builtin unary function on real values.
static Value *eval_builtin_real_unary(CRUnary fn, Value *arg)
{
    CR *x = NULL;
    AS_REAL(arg, x);
    Value *out = value_real(arg->span, fn(x));
    return out;
}

// Evaluate a builtin binary function on real values.
static Value *eval_builtin_real_binary(Value *f, CRBinary fn, Value *right)
{
    Value *left = da_at(&f->as.builtin.args, 0);

    CR *l = NULL, *r = NULL;
    AS_REAL(left, l);
    AS_REAL(right, r);
    Value *out = value_real((Span){left->span.from, right->span.to}, fn(l, r));
    return out;
}

// Evaluate builtin application.
static Value *eval_builtin_apply(Value *f, Value *arg)
{
    if (f->as.builtin.args.len + 1 < f->as.builtin.arity)
    {
        da_append(&f->as.builtin.args, value_retain(arg));
        return value_retain(f);
    }

    switch (f->as.builtin.kind)
    {
        case BUILTIN_TRUE:
        case BUILTIN_FALSE: return eval_builtin_bool(f, arg);
        case BUILTIN_SQRT:  return eval_builtin_real_unary(cr_sqrt, arg);
        case BUILTIN_EXP:   return eval_builtin_real_unary(cr_exp, arg);
        case BUILTIN_LN:    return eval_builtin_real_unary(cr_ln, arg);
        case BUILTIN_POW:   return eval_builtin_real_binary(f, cr_pow, arg);
        case BUILTIN_LOG:   return eval_builtin_real_binary(f, cr_log, arg);

        case BUILTIN_HOLE:  return value_errorf(f->span, "Expected lambda");
        case BUILTIN_ANS:
        default:            UNREACHABLE();
    }
}

// Evaluate an application expression.
static Value *eval_apply(VM *v, Expr *f, Expr *a)
{
    Value *func = vm_eval_expr(v, f);
    if (value_is_err(func)) return func;

    Value *arg = vm_eval_expr(v, a);
    if (value_is_err(arg))
    {
        value_release(func);
        return arg;
    }

    Value *out = NULL;

    switch (func->kind)
    {
        case VAL_LAMBDA:  out = eval_lambda_apply(v, func, arg);  break;
        case VAL_BUILTIN: out = eval_builtin_apply(func, arg);    break;
        default:          out = value_errorf(f->span, "Invalid application of %s", vk_to_str[func->kind]);
                          break;
    }

    value_release(func);
    value_release(arg);
    return out;
}

// Evaluate an infix operation.
static Value *eval_infix(VM *v, Expr *e)
{
    if (e->as.infix.op == OP_ASSIGN)
        return eval_assign_infix(v, e);

    if (e->as.infix.op == OP_APPLY || e->as.infix.op == OP_PIPE)
        return eval_apply(v, e->as.infix.left, e->as.infix.right);

    Value *l = vm_eval_expr(v, e->as.infix.left);
    if (value_is_err(l)) return l;

    Value *r = vm_eval_expr(v, e->as.infix.right);
    if (value_is_err(r))
    {
        value_release(l);
        return r;
    }

    bool same_kind  = l->kind == r->kind;
    bool both_exact = same_kind && l->kind == VAL_EXACT;
    bool both_real  = same_kind && l->kind == VAL_REAL;
    bool one_real   = (l->kind == VAL_REAL && r->kind == VAL_EXACT)
                        || (l->kind == VAL_EXACT && r->kind == VAL_REAL);
    bool both_bool  = same_kind && value_is_bool(l);

    Value *out = NULL;
    if (both_exact)
    {
        out = eval_exact_infix(l, e, r);
    }
    else if (both_real || one_real)
    {
        Value *lr = l;
        Value *rr = r;

        if (l->kind == VAL_EXACT)
            lr = value_real(l->span, cr_from_mpq(l->as.exact));
        if (r->kind == VAL_EXACT)
            rr = value_real(r->span, cr_from_mpq(r->as.exact));

        out = eval_real_infix(lr, e, rr);

        if (l->kind == VAL_EXACT)
            value_release(lr);
        if (l->kind == VAL_EXACT)
            value_release(rr);
    }
    else if (both_bool)
    {
        out = eval_bool_infix(l, e, r);
    }
    else
    {
        out = value_errorf(e->span, "Invalid operation: %s '%s' %s",
                vk_to_str[l->kind], op_to_str[e->as.infix.op], vk_to_str[r->kind]);
    }

    value_release(l);
    value_release(r);
    return out;
}

// Capture free variables from the current scope that are no shadowed by params.
static void capture_free_vars(VM *v, Expr *e, Scope *s)
{
    switch (e->kind)
    {
        case EXPR_IDENT:
            Value *capture = scope_get_symbol(v->scope, SV(e->as.id));
            if (capture)
                scope_set_symbol(s, SV(e->as.id), capture);
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
}

// Evaluate a lambda expression.
static Value *eval_lambda(VM *v, Expr *e)
{
    Scope *s = scope_from(NULL);
    capture_free_vars(v, e->as.lambda.body, s);
    return value_lambda(e, s);
}

// Evaluate a conditional expression.
static Value *eval_cond(VM *v, Expr *e)
{
    Value *cond = vm_eval_expr(v, e->as.cond.if_);
    if (value_is_err(cond)) return cond;

    if (!value_is_bool(cond))
    {
        value_release(cond);
        return value_errorf(e->as.cond.if_->span, "Expected bool");
    }

    Value *out = value_to_bool(cond)
        ? vm_eval_expr(v, e->as.cond.then)
        : vm_eval_expr(v, e->as.cond.else_);

    value_release(cond);
    return out;
}

// Evaluate an expression.
Value *vm_eval_expr(VM *v, Expr *e)
{
    if (is_error(e))
        return value_error_from_expr(e);

    Value *out = NULL;

    switch (e->kind)
    {
        case EXPR_NUMBER: out = eval_number(e);    break;
        case EXPR_IDENT:  out = eval_ident(v, e);  break;
        case EXPR_INFIX:  out = eval_infix(v, e);  break;
        case EXPR_PREFIX: out = eval_prefix(v, e); break;
        case EXPR_LAMBDA: out = eval_lambda(v, e); break;
        case EXPR_COND:   out = eval_cond(v, e);   break;
        default:          UNREACHABLE();
    }
    return out;
}

// Run and evaluate a source.
Value *vm_run(VM *v, StringView src)
{
    Value *out = NULL;

    Module m;
    da_init(&m);

    if (!parse_module(src, v->base, &m))
    {
        out = value_error_from_expr(da_last(&m));
        goto cleanup;
    }

    DA_FOR(&m, i)
    {
        Expr *e = da_at(&m, i);
        out = vm_eval_expr(v, e);
        if (value_is_err(out))
            goto cleanup;

        if (v->last) value_release(v->last);
        v->last = value_retain(out);

        if (i < m.len-1)
            value_release(out);
    }

cleanup:
    module_free(&m);
    return out;
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
        value_render(sym.value, sb, ctx);
        str_append(sb, "\n");
    }
    if (ctx->use_color) str_appendf(sb, AFMT_RESET);
}
