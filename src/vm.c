#include "vm.h"
#include "number.h"
#include "parser.h"

#include <assert.h>
#include <stdarg.h>

// Lookup of string names for value kind.
static const char *vk_to_str[] = {
    [VAL_VOID]    = "void",
    [VAL_ERROR]   = "error",
    [VAL_NUMBER]  = "number",
    [VAL_BOOL]    = "bool",
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
    }
}

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

// Enter a new scope.
static void vm_enter(VM *v)
{
    Scope *s = malloc(sizeof(Scope));
    da_init(s);
    da_append(v->env, s);
}

// Leave the current scope and return to the parent.
static void vm_leave(VM *v)
{
    Scope *s = da_last(v->env);
    scope_free(s);
    v->env->len--;
}

// Initialize a VM with default base;
void vm_init(VM *v)
{
    v->env = malloc(sizeof(Env));
    da_init(v->env);
    vm_enter(v);
    v->last = malloc(sizeof(Value));
    v->base = BASE_DEFAULT;
}

// Reset the state of a VM.
void vm_reset(VM *v)
{
    for (size_t i = 1; i > v->env->len; i++)
        scope_free(da_at(v->env, i));

    da_reset(v->env);
    free(v->last);
    v->last = NULL;
    v->base = BASE_DEFAULT;
}

// Free a VM.
void vm_free(VM *v)
{
    DA_FOR(v->env, i)
    {
        scope_free(da_at(v->env, i));
    }
    da_free(v->env);
    free(v->env);
    free(v->last);
    v->last = NULL;
}

// Assign a symbol to the current scope.
static void symbol_set(VM *v, StringView id, const Value *value)
{
    Scope *scope = da_last(v->env);
    DA_FOR(scope, i)
    {
        Symbol *existing = &da_at(scope, i);
        if (sv_equal(existing->id, id))
        {
            vm_value_free(&existing->value);
            value_set(&existing->value, value);
            return;
        }
    }
    Symbol s = {0};
    s.id = malloc(sizeof(String));
    str_init_with(s.id, id);
    value_set(&s.value, value);
    da_append(scope, s);
}

static bool symbol_get(VM *v, StringView id, Value *out)
{
    Scope *scope = da_last(v->env);
    DA_FOR(scope, i)
    {
        Symbol existing = da_at(scope, i);
        if (sv_equal(existing.id, id))
        {
            value_set(out, &existing.value);
            return true;
        }
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

// Evaluate an identifier
static bool eval_ident(VM *v, Expr *e, Value *out)
{
    if (sv_equal(e->as.id, "@true"))
    {
        value_bool(out, e->span, true);
    }
    else if (sv_equal(e->as.id, "@false"))
    {
        value_bool(out, e->span, false);
    }
    else if (sv_equal(e->as.id, "@@"))
    {
        if (v->last)
            value_set(out, v->last);
    }
    else 
    {
        if (!symbol_get(v, e->as.id, out))
            return errorf(out, e->span, "undefined symbol");
    }
    return true;
}

static bool is_builtin(StringView id)
{
    if (sv_equal(id, "@true")) return true;
    if (sv_equal(id, "@false")) return true;
    if (sv_equal(id, "@@")) return true;
    return false;
}

// Evaluate an assignment expression.
static bool eval_assign(VM *v, Expr *e, Value *out)
{
    Span s = {e->span.from, e->span.from + e->as.assign.id.len};

    if (is_builtin(e->as.assign.id))
        return errorf(out, s, "Cannot assign to builtin symbol");
    if (!vm_eval_expr(v, e->as.assign.expr, out))
        return false;
    symbol_set(v, e->as.assign.id, out);
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

// Evaluate an infix operation.
static bool eval_infix(VM *v, Expr *e, Value *out)
{
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
        case EXPR_ASSIGN: ok = eval_assign(v, e, out); break;
        default:          UNREACHABLE();
    }

    value_set(v->last, out);
    return ok;
}

// Run and evaluate a source.
bool vm_run(VM *v, StringView src, Value *out)
{
    Expr *e = parse(src, v->base);
    bool ok = vm_eval_expr(v, e, out);
    expr_destroy(&e);
    return ok;
}

// Render a number value.
static void number_value_render(Value *v, String *sb, RenderCtx *ctx)
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

// Render a value to the string builder.
// The render may contain newlines but will not have a final newline.
void vm_value_render(Value *v, String *sb, RenderCtx *ctx)
{
    if (v->kind == VAL_ERROR)
    {
        str_appendf(sb, SV_FMT"\n", SV_ARG(ctx->src));
        for (size_t i = 0; i < v->span.from; i++)
            str_append(sb, " ");

        if (ctx->use_color) str_appendf(sb, AFMT_BOLD ACOLOR_MAGENTA);
            for (size_t i = 0; i < v->span.to - v->span.from; i++)
                str_appendf(sb, "^");

            str_appendf(sb, " "SV_FMT, SV_ARG(SV(v->as.error)));
        if (ctx->use_color) str_appendf(sb, AFMT_RESET);
        return;
    }
    if (v->kind == VAL_NUMBER)
    {
        number_value_render(v, sb, ctx);
        return;
    }

    if (ctx->use_color) str_appendf(sb, ACOLOR_YELLOW);
    switch (v->kind)
    {
        case VAL_BOOL:
            str_appendf(sb, "@%s", v->as.boolean ? "true" : "false");
            break;

        default:
            break;
    }
    if (ctx->use_color) str_appendf(sb, AFMT_RESET);
}

void vm_env_render(VM *v, String *sb, RenderCtx *ctx)
{
    DA_FOR(v->env, i)
    {
        str_appendf(sb, "Scope (%zu)\n", i);
        Scope *scope = da_at(v->env, i);
        DA_FOR(scope, j)
        {
            Symbol sym = da_at(scope, j);
            str_appendf(sb, "    "SV_FMT" = ", SV_ARG(SV(sym.id)));
            vm_value_render(&sym.value, sb, ctx);
            str_append(sb, "\n");
        }
        str_append(sb, "\n");
    }
}
