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
    scope_release_r(&v->scope);
    v->scope = scope_from(NULL);
    if (v->last) value_release(&v->last);
    v->last = NULL;
    v->base = BASE_DEFAULT;
}

// Free a VM.
void vm_free(VM *v)
{
    scope_release_r(&v->scope);
    if (v->last) value_release(&v->last);
}

// Evaluate a number expression.
static Value *eval_number(const Expr *e)
{
    return value_exact(e->span, e->as.number);
}

// Evaluate a builtin identifier.
static Value *eval_builtin(VM *v, const Expr *e, BuiltinKind b)
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
static Value *eval_ident(VM *v, const Expr *e)
{
    BuiltinKind builtin = builtin_kind(e);
    if (builtin != BUILTIN_NONE)
        return eval_builtin(v, e, builtin);

    Value *out = scope_get_symbol(v->scope, SV(e->as.id));
    if (!out) return value_errorf(e->span, "Undefined symbol");
    out->span = e->span;
    return out;
}

#define ENSURE_CAPTURE(expr) do { \
    Value *err = capture_free_vars(v, (expr), s, params); \
    if (err) return err; \
} while (0)

// Capture free variables from the current scope that are no shadowed by params.
// Return error or null.
static Value *capture_free_vars(VM *v, const Expr *e, Scope *s, SVList *params)
{
    switch (e->kind)
    {
        case EXPR_IDENT:
            if (svlist_contains(params, SV(e->as.id)))
                break;
            if (builtin_kind(e) != BUILTIN_NONE)
                break;

            Value *capture = scope_get_symbol(v->scope, SV(e->as.id));
            if (capture)
                scope_set_symbol(s, SV(e->as.id), capture);
            else
                return value_errorf(e->span, "Undefined symbol");
            break;

        case EXPR_INFIX:
            if (e->as.infix.op == OP_ASSIGN && e->as.infix.left->kind == EXPR_IDENT)
                da_append(params, SV(e->as.infix.left->as.id));

            ENSURE_CAPTURE(e->as.infix.left);
            ENSURE_CAPTURE(e->as.infix.right);
            break;

        case EXPR_PREFIX:
            ENSURE_CAPTURE(e->as.prefix.expr);
            break;

        case EXPR_LAMBDA:
            if (e->as.lambda.param->kind == EXPR_IDENT)
                da_append(params, SV(e->as.lambda.param->as.id));
            ENSURE_CAPTURE(e->as.lambda.body);
            break;

        case EXPR_COND:
            ENSURE_CAPTURE(e->as.cond.if_);
            ENSURE_CAPTURE(e->as.cond.then);
            ENSURE_CAPTURE(e->as.cond.else_);
            break;

        case EXPR_ERROR:
        case EXPR_NUMBER:
            break;

        default:
            UNREACHABLE();
    }
    return NULL;
}

// Evaluate a lambda expression.
static Value *eval_lambda(VM *v, const Expr *e, StringView name)
{
    SVList p;
    da_init(&p);
    Scope *s = scope_from(NULL);
    if (e->as.lambda.param->kind == EXPR_IDENT)
        da_append(&p, SV(e->as.lambda.param->as.id));

    if (name.len > 0)
    {
        da_append(&p, name);
        scope_set_symbol(s, name, NULL);
    }

    Value *err = capture_free_vars(v, e->as.lambda.body, s, &p);
    if (err)
    {
        da_free(&p);
        scope_release(&s);
        return err;
    }

    Value *out = value_lambda(e, s);
    out->refcount--;
    scope_set_symbol(s, name, out);
    da_free(&p);
    return out;
}

// Evaluate a lambda application.
static Value *eval_lambda_apply(VM *v, const Value *f, Value *arg)
{
    Scope *s = scope_from(f->as.lambda.env);
    Scope *prev = v->scope;
    v->scope = s;

    const Expr *func = f->as.lambda.expr;
    if (func->as.lambda.param->kind == EXPR_IDENT)
        scope_set_symbol(v->scope, SV(func->as.lambda.param->as.id), arg);
    Value *out = vm_eval_expr(v, func->as.lambda.body);
    out->span = f->span;

    v->scope = prev;
    scope_release(&s);
    return out;
}

// Evaluate a builtin bool application.
static Value *eval_builtin_bool(Value *f, Value *arg)
{
    if (!value_to_bool(f)) return value_retain(arg);
    return value_retain(da_at(&f->as.builtin.args, 0));
}

#define AS_REAL(val, cr) do { \
    switch ((val)->kind) { \
        case VAL_REAL:  (cr) = cr_retain((val)->as.real);    break; \
        case VAL_EXACT: (cr) = cr_from_mpq((val)->as.exact); break; \
        default:        return value_error_value_kind_s(val, SV("number")); break; \
    } \
} while (0)

// Evaluate a builtin unary function on real values.
static Value *eval_builtin_real_unary(CRUnary fn, const Value *arg)
{
    CR *x = NULL;
    AS_REAL(arg, x);

    Value *out = NULL;
    if (cr_is_error(x))
    {
        out = value_error_from_cr(arg->span, x);
    }
    else
    {
        CR *n = fn(x);
        if (cr_is_error(n))
            out = value_error_from_cr(arg->span, n);
        else
            out = value_real(arg->span, fn(x));
    }

    cr_release(&x);
    return out;
}

// Evaluate a builtin binary function on real values.
static Value *eval_builtin_real_binary(const Value *f, CRBinary fn, const Value *right)
{
    Value *left = da_at(&f->as.builtin.args, 0);

    Value *out = NULL;
    Span span = {left->span.from, right->span.to};

    CR *l = NULL, *r = NULL;
    AS_REAL(left, l);
    if (cr_is_error(l))
    {
        out = value_error_from_cr(span, l);
        goto done;
    }
    AS_REAL(right, r);
    if (cr_is_error(r))
    {
        out = value_error_from_cr(span, r);
        goto done;
    }

    CR *n = fn(l, r);
    if (cr_is_error(n))
        out = value_error_from_cr(span, n);
    else
        out = value_real(span, fn(l, r));

done:
    cr_release(&l);
    cr_release(&r);
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

        case BUILTIN_HOLE:  return value_error_value_kind_s(f, SV("lambda"));
        case BUILTIN_ANS:
        default:            UNREACHABLE();
    }
}

// Evaluate an application expression.
static Value *eval_apply(VM *v, const Expr *f, const Expr *a)
{
    Value *func = vm_eval_expr(v, f);
    if (value_is_err(func)) return func;

    Value *arg = vm_eval_expr(v, a);
    if (value_is_err(arg))
    {
        value_release(&func);
        return arg;
    }

    Value *out = NULL;

    switch (func->kind)
    {
        case VAL_LAMBDA:  out = eval_lambda_apply(v, func, arg);  break;
        case VAL_BUILTIN: out = eval_builtin_apply(func, arg);    break;
        default:          out = value_error_value_kind(func, VAL_LAMBDA); break;
    }

    value_release(&func);
    value_release(&arg);
    return out;
}


// Perform an mpq infix operation on two numbers wrapped in value.
#define MPQ_INFIX(f, out, l, r) f((out)->as.exact, (l)->as.exact, (r)->as.exact)

// Compare two numbers wrapped in value.
#define MPQ_CMP(l, r) mpq_cmp((l)->as.exact, (r)->as.exact)

// Evaluate one number raised to the power of the other.
static Value *eval_exact_power(const Value *l, const Value *r)
{
    Value *out = NULL;
    Span s = {l->span.from, r->span.to};

    bool non_int = mpz_cmp_ui(mpq_denref(r->as.exact), 1) != 0;
    bool fit_ul = false;
    bool can_render = false;
    if (!non_int)
    {
        fit_ul = mpz_fits_ulong_p(mpq_numref(r->as.exact));
        can_render = bit_estimate_mpq(l->as.exact, r->as.exact) < RENDER_BITS_MAX;
    }

    if (non_int || !fit_ul || !can_render)
    {
        CR *b = cr_from_mpq(l->as.exact);
        CR *x = cr_from_mpq(r->as.exact);
        CR *n = cr_pow(b, x);
        cr_release(&b);
        cr_release(&x);

        if (cr_is_error(n))
        {
            out = value_error_from_cr(r->span, n);
            cr_release(&n);
        }
        else
        {
            out = value_real(s, n);
        }
        return out;
    }

    out = value_exact(s, l->as.exact);

    unsigned long exp = mpz_get_ui(mpq_numref(r->as.exact));
    mpz_pow_ui(mpq_numref(out->as.exact), mpq_numref(l->as.exact), exp);
    mpz_pow_ui(mpq_denref(out->as.exact), mpq_denref(l->as.exact), exp);
    mpq_canonicalize(out->as.exact);
    return out;
}

// Evaluate comparison and equality between exact values.
static Value *eval_exact_bool_infix(const Value *l, const Expr *e, const Value *r)
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
        case OP_APPROX:
            return true;
        default:
            return false;
    }
}

// Evaluate an infix operation between two exact numbers.
static Value *eval_exact_infix(const Value *l, const Expr *e, const Value *r)
{
    if (e->as.infix.op == OP_POW)
        return eval_exact_power(l, r);
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
                value_release(&out);
                return value_errorf(r->span, "Division by zero");
            }
            MPQ_INFIX(mpq_div, out, l, r);
            break;

        default:
            value_release(&out);
            return value_error_undefined_op(l, e, r);
    }
    return out;
}

// Evaluate comparison or equality between two real numbers.
static Value *eval_real_bool_infix(const Value *l, const Expr *e, const Value *r)
{
    bool b;
    switch (e->as.infix.op)
    {
        case OP_APPROX: b = cr_approx(l->as.real, r->as.real); break;
        default:        return value_error_undefined_op(l, e, r);
    }
    return value_bool(e->span, b);
}

// Evaluate infix between two real numbers.
static Value *eval_real_infix(const Value *l, const Expr *e, const Value *r)
{
    if (op_is_cmp_or_eq(e->as.infix.op))
        return eval_real_bool_infix(l, e, r);

    CR *n = NULL;
    switch (e->as.infix.op)
    {
        case OP_ADD: n = cr_add(l->as.real, r->as.real); break;
        case OP_SUB: n = cr_sub(l->as.real, r->as.real); break;
        case OP_MUL: n = cr_mul(l->as.real, r->as.real); break;
        case OP_DIV: n = cr_div(l->as.real, r->as.real); break;

        default:     return value_error_undefined_op(l, e, r);
    }

    if (cr_is_error(n))
    {
        Value *out = value_error_from_cr(e->span, n);
        cr_release(&n);
        return out;
    }
    return value_real(e->span, n);
}

// Evaluate an infix operation between two booleans.
static Value *eval_bool_infix(const Value *l, const Expr *e, const Value *r)
{
    bool lb = value_to_bool(l);
    bool rb = value_to_bool(r);
    bool vb = false;

    switch (e->as.infix.op)
    {
        case OP_EQ:  vb = lb == rb; break;
        case OP_NEQ: vb = lb != rb; break;
        default:     return value_error_undefined_op(l, e, r);
    }
    return value_bool(e->span, vb);
}

// Evaluate an assignment infix operation.
static Value *eval_assign_infix(VM *v, const Expr *e)
{
    const Expr *l = e->as.infix.left;
    if (l->kind != EXPR_IDENT)
        return value_error_expr_kind(l, EXPR_IDENT);

    BuiltinKind builtin = builtin_kind(l);
    if (builtin != BUILTIN_NONE && builtin != BUILTIN_HOLE)
        return value_errorf(l->span, "Cannot assign to builtin identifier");

    Value *out = NULL;
    if (e->as.infix.right->kind == EXPR_LAMBDA)
        out = eval_lambda(v, e->as.infix.right, SV(l->as.id));
    else
        out = vm_eval_expr(v, e->as.infix.right);

    if (value_is_err(out)) return out;

    if (builtin != BUILTIN_HOLE)
        scope_set_symbol(v->scope, SV(l->as.id), out);

    return out;
}

// Evaluate an infix operation.
static Value *eval_infix(VM *v, const Expr *e)
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
        value_release(&l);
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
        Value *lr = value_retain(l);
        Value *rr = value_retain(r);

        if (l->kind == VAL_EXACT)
            lr = value_real(l->span, cr_from_mpq(l->as.exact));
        if (r->kind == VAL_EXACT)
            rr = value_real(r->span, cr_from_mpq(r->as.exact));

        out = eval_real_infix(lr, e, rr);

        if (l->kind == VAL_EXACT)
            value_release(&lr);
        if (l->kind == VAL_EXACT)
            value_release(&rr);
    }
    else if (both_bool)
    {
        out = eval_bool_infix(l, e, r);
    }
    else
    {
        out = value_error_undefined_op(l, e, r);
    }

    value_release(&l);
    value_release(&r);
    return out;
}

// Evaluate a prefix expression.
static Value *eval_prefix(VM *v, const Expr *e)
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

    Value *err = value_error_undefined_op(NULL, e, out);
    value_release(&out);
    return err;
}

// Evaluate a conditional expression.
static Value *eval_cond(VM *v, const Expr *e)
{
    Value *cond = vm_eval_expr(v, e->as.cond.if_);
    if (value_is_err(cond)) return cond;

    if (!value_is_bool(cond))
    {
        Value *err =  value_error_value_kind_s(cond, SV("bool"));
        value_release(&cond);
        return err;
    }

    Value *out = value_to_bool(cond)
        ? vm_eval_expr(v, e->as.cond.then)
        : vm_eval_expr(v, e->as.cond.else_);

    value_release(&cond);
    return out;
}

// Evaluate an expression.
Value *vm_eval_expr(VM *v, const Expr *e)
{
    if (is_error(e))
        return value_error_from_expr(e);

    Value *out = NULL;

    switch (e->kind)
    {
        case EXPR_NUMBER: out = eval_number(e);            break;
        case EXPR_IDENT:  out = eval_ident(v, e);          break;
        case EXPR_INFIX:  out = eval_infix(v, e);          break;
        case EXPR_PREFIX: out = eval_prefix(v, e);         break;
        case EXPR_LAMBDA: out = eval_lambda(v, e, SV("")); break;
        case EXPR_COND:   out = eval_cond(v, e);           break;
        default:          UNREACHABLE();
    }
    DEV_MUST(out);
    return out;
}

static Value *vm_run_module(VM *v, StringView src, Module *m)
{
    Value *out = NULL;

    if (!parse_module(src, v->base, m))
    {
        ModuleEntry err = da_last(m);
        return value_error_from_expr(err.expr);
    }

    DA_FOR(m, i)
    {
        ModuleEntry entry = da_at(m, i);
        const Expr *e = entry.expr;

        out = vm_eval_expr(v, e);
        if (value_is_err(out))
            return out;

        if (v->last) value_release(&v->last);
        v->last = value_retain(out);

        if (i < m->len-1)
            value_release(&out);
    }
    return out;
}

Value *vm_run_render(VM *v, StringView src, String *sb, RenderCtx *ctx)
{
    Module m;
    da_init(&m);

    Value *out = vm_run_module(v, src, &m);
    ModuleEntry last = da_last(&m);

    ctx->src = SV(last.src);
    value_render(out, sb, ctx);

    module_free(&m);
    return out;
}

// Run and evaluate a source.
Value *vm_run(VM *v, StringView src)
{
    Module m;
    da_init(&m);

    Value *out = vm_run_module(v, src, &m);

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

    if (v->last)
    {
        str_append(sb, "    ans = ");
        value_render(v->last, sb, ctx);
        str_append(sb, "\n");
    }

    DA_FOR(v->scope, i)
    {
        Symbol sym = da_at(v->scope, i);
        str_appendf(sb, "    "SV_FMT" = ", SV_ARG(SV(sym.id)));
        value_render(sym.value, sb, ctx);
        str_append(sb, "\n");
    }

    if (ctx->use_color) str_appendf(sb, AFMT_RESET);
}
