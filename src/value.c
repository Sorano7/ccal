#include "value.h"
#include "number.h"

// Lookup of string names for value kind.
const char *vk_to_str[] = {
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
    [BUILTIN_POW]   = "pow",
    [BUILTIN_EXP]   = "exp",
    [BUILTIN_LOG]   = "log",
    [BUILTIN_LN]    = "ln",
};

// Intializes an exact value.
void value_exact(Value *v, Span span)
{
    DEV_MUST(v);
    v->kind = VAL_EXACT;
    v->span = span;
    mpq_init(v->as.exact);
}

// Initializes a real value.
void value_real(Value *v, Span span, CR *n)
{
    DEV_MUST(v);
    v->kind = VAL_REAL;
    v->span = span;
    v->as.real = n;
}

// Intializes a builtin value.
void value_builtin(Value *v, Span span, BuiltinKind kind, size_t arity)
{
    DEV_MUST(v);
    v->kind = VAL_BUILTIN;
    v->span = span;
    v->as.builtin.kind = kind;
    v->as.builtin.arity = arity;
    da_init(&v->as.builtin.args);
}

// Intializes a bool value.
void value_bool(Value *v, Span span, bool b)
{
    value_builtin(v, span, b ? BUILTIN_TRUE : BUILTIN_FALSE, 2);
}

// Initializes a lambda value.
void value_lambda(Value *v, Expr *e, Scope *s)
{
    DEV_MUST(v);
    DEV_MUST(e->kind == EXPR_LAMBDA);
    v->kind = VAL_LAMBDA;
    v->span = e->span;
    v->as.lambda.expr = expr_clone(e);
    v->as.lambda.env = s;
}

// Initializes an error value with message.
bool value_errorf(Value *v, Span span, const char *fmt, ...)
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
bool value_error_from_expr(Value *v, const Expr *e)
{
    DEV_MUST(is_error(e));

    v->kind = VAL_ERROR;
    v->span = e->span;
    str_init_with(&v->as.error, &e->as.err);

    return false;
}

// Set a value from another value.
void value_set(Value *v, const Value *from)
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
            v->as.real = cr_retain(from->as.real);
            break;

        case VAL_LAMBDA:
            Expr *l = from->as.lambda.expr;
            v->as.lambda.expr = expr_lambda(l->as.lambda.param, l->as.lambda.body);
            v->as.lambda.env = from->as.lambda.env;
            v->as.lambda.env->refcount++;
            break;

        case VAL_BUILTIN:
            v->as.builtin.kind = from->as.builtin.kind;
            v->as.builtin.arity = from->as.builtin.arity;
            da_clone(&v->as.builtin.args, &from->as.builtin.args);
            break;

        default:
            UNREACHABLE();
    }
}

// Free the value.
void value_free(Value *v)
{
    if (!v) return;
    switch (v->kind)
    {
        case VAL_EXACT:
            mpq_clear(v->as.exact);
            break;

        case VAL_REAL:
            cr_release(v->as.real);
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
            da_free(&v->as.builtin.args);
            break;

        case VAL_VOID:
            break;

        default:
            UNREACHABLE();
    }
    memset(v, 0, sizeof(*v));
}

// Checks if a value is a bool value.
bool value_is_bool(const Value *v)
{
    if (v->kind != VAL_BUILTIN)
        return false;
    return v->as.builtin.kind == BUILTIN_TRUE
        || v->as.builtin.kind == BUILTIN_FALSE;
}

// Use a builtin boolean value as bool.
bool value_to_bool(const Value *v)
{
    DEV_MUST(value_is_bool(v));
    return v->as.builtin.kind == BUILTIN_TRUE;
}

// Get the builtin kind from an expression
BuiltinKind builtin_kind(Expr *e)
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
    if (sv_equal(name, "pow"))   return BUILTIN_POW;
    if (sv_equal(name, "exp"))   return BUILTIN_EXP;
    if (sv_equal(name, "log"))   return BUILTIN_LOG;
    if (sv_equal(name, "ln"))    return BUILTIN_LN;

    return BUILTIN_NONE;
}

// Free a scope and all of its symbols.
void scope_free(Scope *s)
{
    DA_FOR(s, i)
    {
        Symbol sym = da_at(s, i);
        str_free(sym.id);
        free(sym.id);
        value_free(&sym.value);
    }
    da_free(s);
}

// Recursively free a scope.
void scope_free_r(Scope *s)
{
    scope_free(s);
    while ((s = s->parent))
        scope_free(s);
}

// Create a new scope from a parent.
Scope *scope_from(Scope *parent)
{
    Scope *s = malloc(sizeof(Scope));
    da_init(s);
    s->parent = parent;
    s->refcount = 0;
    return s;
}

// Assign a symbol to the scope.
void symbol_set(Scope *scope, StringView id, const Value *value)
{
    DA_FOR(scope, i)
    {
        Symbol *existing = &da_at(scope, i);
        if (sv_equal(existing->id, id))
        {
            value_free(&existing->value);
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
bool symbol_get(Scope *scope, StringView id, Value *out)
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

// Render an exact value.
static void value_render_exact(Value *v, String *sb, RenderCtx *ctx)
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

    int applied = v->as.builtin.args.len;
    int needs = v->as.builtin.arity - applied;

    bool as_lambda = applied > 0;

    if (as_lambda)
    {
        for (int i = 0; i < needs; i++)
            str_appendf(sb, "('a%d : ", i+1);
    }

    str_appendf(sb, "'%s", builtin_to_str[v->as.builtin.kind]);

    if (as_lambda)
    {
        for (int i = 0; i < applied; i++)
        {
            str_append(sb, " ");
            value_render(&da_at(&v->as.builtin.args, i), sb, ctx);
            if (ctx->use_color) str_appendf(sb, ACOLOR_YELLOW);
        }

        for (int i = 0; i < needs; i++)
            str_appendf(sb, " 'a%d", i+1);

        for (int i = 0; i < needs; i++)
            str_append(sb, ")");
    }

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
                    value_render(&tmp, sb, ctx);
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
    value_free(&tmp);
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

// Compute and render a real value.
static void value_render_real(Value *v, String *sb, RenderCtx *ctx)
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
void value_render(Value *v, String *sb, RenderCtx *ctx)
{
    switch (v->kind)
    {
        case VAL_ERROR:   return value_render_error(v, sb, ctx);
        case VAL_EXACT:   return value_render_exact(v, sb, ctx);
        case VAL_REAL:    return value_render_real(v, sb, ctx);
        case VAL_BUILTIN: return value_render_builtin(v, sb, ctx);
        case VAL_LAMBDA:  return value_render_lambda(v, sb, ctx);
        case VAL_VOID:    break;
        default:          UNREACHABLE();
    }
}
