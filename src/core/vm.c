#include "vm.h"
#include "number.h"
#include "parser.h"
#include <stdarg.h>

NATIVE_FN(native_true)
{
    (void)v, (void)ud;
    return argv[0];
}

NATIVE_FN(native_false)
{
    (void)v, (void)ud;
    return argv[1];
}

static void register_builtin_natives(VM *v)
{
    vm_set_native(v, SV("true"),  native_true,  2, NULL);
    vm_set_native(v, SV("false"), native_false, 2, NULL);
}

void vm_set_native(VM *v, StringView id, NativeFn fn, size_t arity, void *ud)
{
    NativeEntry e = {
        .fn    = fn,
        .arity = arity,
        .ud    = ud,
    };
    str_init_with(&e.id, id);
    da_append(&v->natives, e);
}

static void vm_ctx_default(VM *v)
{
    v->ctx.ibase         = BASE_DEFAULT;
    v->ctx.obase         = BASE_DEFAULT;
    v->ctx.prec          = 50;
    v->ctx.max_digits    = 10;
    v->ctx.fmt           = FMT_AUTO;
    v->ctx.show_rational = false;
    v->ctx.use_color     = false;
}

// Initialize a VM with default base;
void vm_init(VM *v)
{
    v->scope = scope_from(NULL);
    v->last = NULL;

    vm_ctx_default(v);

    da_init(&v->natives);
    register_builtin_natives(v);
}

// Reset the state of a VM.
void vm_reset(VM *v)
{
    scope_reset(v->scope);
    if (v->last) value_release(&v->last);

    vm_ctx_default(v);

    da_reset(&v->natives);
    register_builtin_natives(v);
}

// Free a VM.
void vm_free(VM *v)
{
    scope_release_r(&v->scope);
    if (v->last) value_release(&v->last);
}

static Value *eval_expr(VM *v, const Expr *e);

// Evaluate a number expression.
static Value *eval_number(const Expr *e)
{
    return value_exact(e->span, e->as.number);
}

// Evaluate an identifier
static Value *eval_ident(VM *v, const Expr *e)
{
    DA_FOREACH(&v->natives, NativeEntry, entry)
    {
        if (sv_equal(e->as.id, entry->id))
            return value_native(entry->fn, entry->arity, entry->ud);
    }

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
    Value *out = eval_expr(v, func->as.lambda.body);

    v->scope = prev;
    scope_release(&s);
    return out;
}

#define ENSURE_REAL(val, cr) do { \
    switch ((val)->kind) { \
        case VAL_REAL:  (cr) = cr_retain((val)->as.real);    break; \
        case VAL_EXACT: (cr) = cr_from_mpq((val)->as.exact); break; \
        default:        return value_error_value_kind_s(val, SV("number")); break; \
    } \
} while (0)

// // Evaluate a builtin unary function on real values.
// static Value *eval_builtin_real_unary(CRUnary fn, const Value *arg)
// {
//     CR *x = NULL;
//     ENSURE_REAL(arg, x);
//     if (cr_is_error(x))
//         return value_error_from_cr(arg->span, x);
//
//     Value *out = NULL;
//
//     CR *n = fn(x);
//     if (cr_is_error(n))
//         out = value_error_from_cr(arg->span, n);
//     else
//         out = value_real(arg->span, fn(x));
//
//     cr_release(&x);
//     return out;
// }
//
// // Evaluate a builtin binary function on real values.
// static Value *eval_builtin_real_binary(const Value *f, CRBinary fn, const Value *right)
// {
//     Value *left = f->as.builtin.args[0];
//
//     Value *out = NULL;
//     Span span = {left->span.from, right->span.to};
//
//     CR *l = NULL, *r = NULL;
//     ENSURE_REAL(left, l);
//     if (cr_is_error(l))
//     {
//         out = value_error_from_cr(span, l);
//         goto done;
//     }
//     ENSURE_REAL(right, r);
//     if (cr_is_error(r))
//     {
//         out = value_error_from_cr(span, r);
//         goto done;
//     }
//
//     CR *n = fn(l, r);
//     if (cr_is_error(n))
//         out = value_error_from_cr(span, n);
//     else
//         out = value_real(span, fn(l, r));
//
// done:
//     cr_release(&l);
//     cr_release(&r);
//     return out;
// }

// // Evaluate builtin application.
// static Value *eval_builtin_apply(Value *f, Value *arg)
// {
//     if (f->as.builtin.len + 1 < f->as.builtin.arity)
//     {
//         f->as.builtin.args[f->as.builtin.len++] = value_retain(arg);
//         return value_retain(f);
//     }
//
//     switch (f->as.builtin.kind)
//     {
//         case BUILTIN_TRUE:
//         case BUILTIN_FALSE: return eval_builtin_bool(f, arg);
//         case BUILTIN_SQRT:  return eval_builtin_real_unary(cr_sqrt, arg);
//         case BUILTIN_EXP:   return eval_builtin_real_unary(cr_exp, arg);
//         case BUILTIN_LN:    return eval_builtin_real_unary(cr_ln, arg);
//         case BUILTIN_POW:   return eval_builtin_real_binary(f, cr_pow, arg);
//         case BUILTIN_LOG:   return eval_builtin_real_binary(f, cr_log, arg);
//         case BUILTIN_MOD:   return eval_builtin_real_binary(f, cr_mod, arg);
//
//         case BUILTIN_HOLE:  return value_error_value_kind_s(f, SV("lambda"));
//         case BUILTIN_ANS:
//         default:            UNREACHABLE();
//     }
// }

static Value *eval_native_apply(VM *v, Value *f, Value *arg)
{
    if (f->as.native.argc < f->as.native.arity)
    {
        f->as.native.argv[f->as.native.argc++] = value_retain(arg);

        // Clone instead of retain
        if (f->as.native.argc < f->as.native.arity)
            return value_retain(f);
    }

    return f->as.native.fn(v, f->as.native.argv, f->as.native.ud);
}

// Evaluate an application expression.
static Value *eval_apply(VM *v, const Expr *f, const Expr *a)
{
    Value *func = eval_expr(v, f);
    if (value_is_err(func)) return func;

    Value *arg = eval_expr(v, a);
    if (value_is_err(arg))
    {
        value_release(&func);
        return arg;
    }

    Value *out = NULL;

    switch (func->kind)
    {
        case VAL_LAMBDA:  out = eval_lambda_apply(v, func, arg);          break;
        case VAL_NATIVE:  out = eval_native_apply(v, func, arg);          break;
        default:          out = value_error_value_kind(func, VAL_LAMBDA); break;
    }

    value_release(&func);
    value_release(&arg);
    return out;
}

// Compare two numbers wrapped in value.
#define MPQ_CMP(l, r) mpq_cmp((l)->as.exact, (r)->as.exact)

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

static Value *eval_exact_power(const Value *l, const Value *r)
{
    Value *out = NULL;
    Span s = {l->span.from, r->span.to};

    bool is_int = mpz_cmp_ui(mpq_denref(r->as.exact), 1) == 0;
    bool fit_ul = mpz_fits_ulong_p(mpq_numref(r->as.exact));
    bool can_render = bit_estimate_mpq(l->as.exact, r->as.exact) < RENDER_BITS_MAX;

    if (is_int && fit_ul && can_render)
    {
        out = value_exact(s, l->as.exact);
        unsigned long exp = mpz_get_ui(mpq_numref(r->as.exact));
        mpz_pow_ui(mpq_numref(out->as.exact), mpq_numref(l->as.exact), exp);
        mpz_pow_ui(mpq_denref(out->as.exact), mpq_denref(l->as.exact), exp);
        mpq_canonicalize(out->as.exact);
        return out;
    }

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

static Value *eval_exact_modulo(const Value *l, const Expr *e, const Value *r)
{
    if (mpq_cmp_ui(r->as.exact, 0, 1) == 0)
        return value_errorf(r->span, "Modulo by zero");

    mpq_t result, q, nb;
    mpz_t n;

    mpq_inits(result, q, nb, NULL);
    mpz_init(n);

    mpq_div(q, l->as.exact, r->as.exact);
    mpz_fdiv_q(n, mpq_numref(q), mpq_denref(q));

    mpq_set_z(nb, n);
    mpq_mul(nb, nb, r->as.exact);
    mpq_sub(result, l->as.exact, nb);

    Value *out = value_exact(e->span, result);

    mpq_clears(result, q, nb, NULL);
    mpz_clear(n);
    return out;
}

// Perform an mpq infix operation on two numbers wrapped in value.
#define MPQ_INFIX(f, out, l, r) f((out)->as.exact, (l)->as.exact, (r)->as.exact)

// Evaluate an infix operation between two exact numbers.
static Value *eval_exact_infix(const Value *l, const Expr *e, const Value *r)
{
    if (e->as.infix.op == OP_POW)
        return eval_exact_power(l, r);
    if (e->as.infix.op == OP_MOD)
        return eval_exact_modulo(l, e, r);

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

    CR *a = l->as.real;
    CR *b = r->as.real;

    CR *n = NULL;
    switch (e->as.infix.op)
    {
        case OP_ADD: n = cr_add(a, b); break;
        case OP_SUB: n = cr_sub(a, b); break;
        case OP_MUL: n = cr_mul(a, b); break;
        case OP_DIV: n = cr_div(a, b); break;
        case OP_POW: n = cr_pow(a, b); break;
        case OP_MOD: n = cr_mod(a, b); break;

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

    Value *out = NULL;
    if (e->as.infix.right->kind == EXPR_LAMBDA)
        out = eval_lambda(v, e->as.infix.right, SV(l->as.id));
    else
        out = eval_expr(v, e->as.infix.right);

    if (value_is_err(out)) return out;
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

    Value *l = eval_expr(v, e->as.infix.left);
    if (value_is_err(l)) return l;

    Value *r = eval_expr(v, e->as.infix.right);
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
    Value *out = eval_expr(v, e->as.prefix.expr);
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
    Value *cond = eval_expr(v, e->as.cond.if_);
    if (value_is_err(cond)) return cond;

    if (!value_is_bool(cond))
    {
        Value *err =  value_error_value_kind_s(cond, SV("bool"));
        value_release(&cond);
        return err;
    }

    Value *out = value_to_bool(cond)
        ? eval_expr(v, e->as.cond.then)
        : eval_expr(v, e->as.cond.else_);

    value_release(&cond);
    return out;
}

// Evaluate an expression.
Value *eval_expr(VM *v, const Expr *e)
{
    DEV_MUST(!expr_is_err(e));

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

// Separate into semicolon-separated expressions.
static void collect_exprs(SVList *exprs, StringView src)
{
    while (src.len > 0)
    {
        StringView expr = sv_split(&src, ';');
        da_append(exprs, expr);
    }
}

static void collect_lines(SVList *lines, StringView src)
{
    while (src.len > 0)
    {
        size_t newline = sv_find(src, '\n');
        if (newline == SIZE_MAX) newline = src.len;
        else                     newline++;

        StringView line = sv_shift(&src, newline);
        da_append(lines, line);
    }
}

static Value *vm_run_expr(VM *v, Expr *e)
{
    if (expr_is_err(e))
        return value_error_from_expr(e);
    if (expr_is_incomplete(e))
        return value_errorf(e->span, "Incomplete expression");

    Value *out = eval_expr(v, e);
    DEV_MUST(out);

    if (value_is_err(out)) return out;

    if (v->last) value_release(&v->last);
    v->last = value_retain(out);

    return out;
}

bool vm_is_complete(VM *v, StringView src)
{
    Expr *e = parse(src, v->ctx.ibase);
    if (!e) return true;

    bool complete = !expr_is_incomplete(e);
    expr_destroy(&e);
    return complete;
}

Value *vm_run_next(VM *v, StringView src, size_t offset)
{
    SVList exprs;
    da_init(&exprs);
    collect_exprs(&exprs, src);

    int vals_count = 0;
    Value *vals[exprs.len];

    DA_FOREACH(&exprs, StringView, line)
    {
        if (sv_trim(*line).len == 0)
            continue;

        Expr *e = parse_line(sv_trim(*line), v->ctx.ibase, offset);
        if (!e) continue;

        Value *next = vm_run_expr(v, e);
        expr_destroy(&e);

        vals[vals_count++] = next;
        if (value_is_err(next)) break;

        offset += line->len;
    }

    da_free(&exprs);

    if (vals_count == 0)
        return value_void((Span){0, src.len});

    for (int i = 0; i < vals_count-1; i++)
        value_release(&vals[i]);

    Value *out = vals[vals_count-1];
    DEV_MUST(out);
    return out;
}

Value *vm_run(VM *v, StringView input, Source *src)
{
    SVList lines;
    da_init(&lines);
    collect_lines(&lines, input);

    String in;
    str_init(&in);

    int vals_count = 0;
    Value *vals[lines.len];

    DA_FOR(&lines, i)
    {
        size_t offset = src ? source_get_offset(src) : 0;

        StringView line = da_at(&lines, i);
        if (sv_trim(line).len == 0)
            continue;

        str_append(&in, line);
        while (!vm_is_complete(v, SV(in)))
        {
            line = da_at(&lines, ++i);
            str_append(&in, line);
        }
        if (src) source_append_line(src, SV(in));

        Value *next = vm_run_next(v, SV(in), offset);

        if (next->kind != VAL_VOID)
            vals[vals_count++] = next;
        if (value_is_err(next)) break;

        str_reset(&in);
    }

    str_free(&in);
    da_free(&lines);

    if (vals_count == 0)
        return value_void((Span){0, input.len});

    for (int i = 0; i < vals_count-1; i++)
        value_release(&vals[i]);

    Value *out = vals[vals_count-1];
    DEV_MUST(out);
    return out;
}

#define appendc(c) do { \
    if (v->ctx.use_color) str_append(sb, (c)); \
} while (0)

// Render an exact value.
static void value_render_exact(VM *v, Value *val, String *sb)
{
    const unsigned long obase = v->ctx.obase;
    const unsigned long ibase = v->ctx.ibase;

    if (obase >= 62)
    {
        appendc(ACOLOR_MAGENTA);
        str_append(sb, "Output base too large");
        appendc(AFMT_RESET);
        return;
    }

    if (obase != ibase)
    {
        appendc(AFMT_DIM);
        str_appendf(sb, "%lu#", obase);
        appendc(AFMT_RESET);
    }

    appendc(ACOLOR_YELLOW);

    render_mpq_as_decimal(sb, val->as.exact, obase, v->ctx.max_digits);

    bool den_is_one = mpz_cmp_ui(mpq_denref(val->as.exact), 1) == 0;
    if (v->ctx.show_rational && !den_is_one)
    {
        appendc(AFMT_RESET AFMT_DIM);

        char *s = mpq_get_str(NULL, obase, val->as.exact);
        str_appendf(sb, " or %s", s);
        free(s);
    }

    appendc(AFMT_RESET);
}

static void render_error_carets(VM *v, Span span, String *sb)
{

    for (size_t i = 0; i < span.from; i++)
        str_append(sb, " ");

    appendc(AFMT_BOLD ACOLOR_MAGENTA);

    size_t len = span.to - span.from;
    for (size_t i = 0; i < len; i++)
        str_appendf(sb, "^");

    str_appendf(sb, " ");
    appendc(AFMT_RESET);
}

// Render an error value.
static void value_render_error(VM *v, Value *val, String *sb)
{
    String src_str;
    str_init(&src_str);

    Span err_span = val->span;
    Source *src = v->ctx.src;
    bool show_src = src 
        && source_get_line(src, val->span, &err_span, &src_str)
        && src_str.len > 0;

    bool msg_shown = false;
    if (show_src)
    {
        StringView expr = SV(src_str);
        size_t start = 0;
        while (expr.len > 0)
        {
            StringView line = sv_split(&expr, '\n');
            str_appendf(sb, SV_FMT"\n", SV_ARG(line));

            size_t end = start + line.len + 1;
            bool overlap = err_span.from <= start || end >= err_span.to;
            if (!msg_shown && overlap)
            {
                Span carets = {
                    err_span.from > start ? err_span.from-start : 0,
                    err_span.to < end ? err_span.to-start : line.len,
                };
                render_error_carets(v, carets, sb);

                if (end >= err_span.to)
                {
                    appendc(AFMT_BOLD ACOLOR_MAGENTA);
                    str_append(sb, SV(val->as.error));
                    appendc(AFMT_RESET);
                    msg_shown = true;
                }
                str_append(sb, "\n");
            }
            start += line.len + 1;
        }
    }

    if (!msg_shown)
    {
        appendc(AFMT_BOLD ACOLOR_MAGENTA);
        str_append(sb, SV(val->as.error));
        appendc(AFMT_RESET);
    }
}

// Render a builtin value.
static void value_render_native(VM *v, Value *val, String *sb)
{
    str_append(sb, "<native>");
}

#define RENDER_SUBST(e) render_with_subst(v, s, (e), params, sb)

// Render an expression with identifiers substituted.
static void render_with_subst(VM *v, Scope *s, Expr *e, SVList *params, String *sb)
{
    switch (e->kind)
    {
        case EXPR_LAMBDA:
            str_appendf(sb, "(");
            expr_render(e->as.lambda.param, sb);
            if (e->as.lambda.param->kind == EXPR_IDENT)
                da_append(params, SV(e->as.lambda.param->as.id));
            str_appendf(sb, ": ");
            RENDER_SUBST(e->as.lambda.body);
            str_appendf(sb, ")");
            break;

        case EXPR_IDENT:
            Value *existing = scope_get_symbol(s, SV(e->as.id));
            if (!existing)
            {
                expr_render(e, sb);
                break;
            }

            bool is_self = existing->kind == VAL_LAMBDA 
                && existing->as.lambda.env == s;
            bool is_param = svlist_contains(params, SV(e->as.id));

            if (is_self || is_param)
            {
                expr_render(e, sb);
                break;
            }

            vm_value_render(v, existing, sb);
            appendc(ACOLOR_YELLOW);
            value_release(&existing);
            break;

        case EXPR_PREFIX:
            str_appendf(sb, " %s", op_to_str[e->as.prefix.op]);
            RENDER_SUBST(e->as.prefix.expr);
            break;

        case EXPR_INFIX:
            RENDER_SUBST(e->as.infix.left);
            if (e->as.infix.op == OP_APPLY)
                str_appendf(sb, " ");
            else
                str_appendf(sb, " %s ", op_to_str[e->as.infix.op]);
            RENDER_SUBST(e->as.infix.right);
            break;

        case EXPR_COND:
            RENDER_SUBST(e->as.cond.if_);
            str_appendf(sb, " ? ");
            RENDER_SUBST(e->as.cond.then);
            str_appendf(sb, " : ");
            RENDER_SUBST(e->as.cond.else_);
            break;

        default:
            expr_render(e, sb);
            break;
    }
}

// Render a lambda value;
static void value_render_lambda(VM *v, Value *val, String *sb)
{
    appendc(ACOLOR_YELLOW);

    SVList sl;
    da_init(&sl);

    Expr *e = val->as.lambda.expr;
    if (e->as.lambda.param->kind == EXPR_IDENT)
        da_append(&sl, SV(e->as.lambda.param->as.id));
    render_with_subst(v, val->as.lambda.env, e, &sl, sb);

    da_free(&sl);
    appendc(AFMT_RESET);
}

// Compute and render a real value.
static void value_render_real(VM *v, Value *val, String *sb)
{
    VMCtx *ctx = &v->ctx;

    mpfi_t result;
    mpfi_init2(result, ctx->prec);
    cr_eval(val->as.real, ctx->prec, result);

    appendc(AFMT_DIM);
    str_append(sb, "~= ");
    appendc(AFMT_RESET);

    appendc(ACOLOR_YELLOW);
    render_mpfi(sb, result, ctx->obase, ctx->max_digits, ctx->fmt);
    appendc(AFMT_RESET);

    mpfi_clear(result);
}

// Render a value to the string builder.
// The render may contain newlines but will not have a final newline.
void vm_value_render(VM *v, Value *val, String *sb)
{
    switch (val->kind)
    {
        case VAL_ERROR:   return value_render_error(v, val, sb);
        case VAL_EXACT:   return value_render_exact(v, val, sb);
        case VAL_REAL:    return value_render_real(v, val, sb);
        case VAL_NATIVE:  return value_render_native(v, val, sb);
        case VAL_LAMBDA:  return value_render_lambda(v, val, sb);
        case VAL_VOID:    break;
        default:          UNREACHABLE();
    }
}

static void render_symbol_id(VM *v, String *sb, StringView id, size_t max_len)
{
    str_append(sb, "    ");

    appendc(ACOLOR_CYAN);
    str_append(sb, id);
    for (size_t i = 0; i < max_len - id.len; i++)
        str_append(sb, " ");

    appendc(AFMT_RESET AFMT_DIM);
    str_append(sb, " = ");
    appendc(AFMT_RESET);
}

// Render the current environment of the VM.
void vm_env_render(VM *v, String *sb)
{
    Scope *s = v->scope;
    Value *ans = v->last;

    if (s->len == 0 && !ans)
    {
        appendc(ACOLOR_CYAN);
        str_appendf(sb, "Empty.\n");
        appendc(AFMT_RESET);
        return;
    }

    size_t max_len = ans ? 3 : 0;
    DA_FOREACH(s, Symbol, sym)
    {
        size_t len = sym->id->len;
        if (len > max_len) max_len = len;
    }

    if (ans)
    {
        appendc(AFMT_RESET);
        render_symbol_id(v, sb, SV("ans"), max_len);
        vm_value_render(v, ans, sb);
        str_append(sb, "\n");
    }

    DA_FOREACH(s, Symbol, sym)
    {
        render_symbol_id(v, sb,SV(sym->id), max_len);
        vm_value_render(v, sym->value, sb);
        str_append(sb, "\n");
    }

    appendc(AFMT_RESET);
}

