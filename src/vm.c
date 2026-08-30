#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "vm.h"
#include "number.h"

#define BASE_DEFAULT 10
#define BASE_MAX ULONG_MAX

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
            str_free(&v->as.error.msg);
            break;

        default:
            break;
    }
    memset(v, 0, sizeof(*v));
}

// Intializes a number value.
static void value_number(Value *v)
{
    assert(v);
    v->kind = VAL_NUMBER;
    mpq_init(v->as.number);
}

// Initializes a bool value.
static void value_bool(Value *v, bool b)
{
    assert(v);
    v->kind = VAL_BOOL;
    v->as.boolean = b;
}

// Create a formatted error value pointing to the position.
static bool value_errorf(Value *v, size_t pos, const char *fmt, ...)
{
    vm_value_free(v);

    v->kind = VAL_ERROR;
    v->as.error.pos = pos;
    str_init(&v->as.error.msg);

    va_list args;
    va_start(args, fmt);

    str_appendvf(&v->as.error.msg, fmt, args);

    va_end(args);
    return false;
}

// Set a value from another value.
static void value_set(Value *v, const Value *from)
{
    switch (from->kind)
    {
        case VAL_VOID:
            v->kind = VAL_VOID;
            break;

        case VAL_NUMBER:
            value_number(v);
            mpq_set(v->as.number, from->as.number);
            mpq_canonicalize(v->as.number);
            break;

        case VAL_BOOL:
            value_bool(v, from->as.boolean);
            break;

        case VAL_ERROR:
            StringView s = SV(from->as.error.msg);
            value_errorf(v, from->as.error.pos, SV_FMT, SV_ARG(SV(s)));
            break;
    }
}

// Operator precedence levels.
typedef enum
{
    PREC_PRIMARY,

    PREC_EQUALITY,
    PREC_COMPARISON,

    PREC_SUM,
    PREC_PRODUCT,
    PREC_POWER,

    PREC_PREFIX,
    PREC_BASE,
} OpPrec;

// Get the precedence of the token.
static OpPrec token_get_prec(Token t)
{
    switch (t.kind)
    {
        case TOK_EQ:
        case TOK_NEQ:
            return PREC_EQUALITY;

        case TOK_LT:
        case TOK_LEQ:
        case TOK_GT:
        case TOK_GEQ:
            return PREC_COMPARISON;

        case TOK_PLUS:
        case TOK_MINUS:
            return PREC_SUM;

        case TOK_STAR:
        case TOK_SLASH:
            return PREC_PRODUCT;

        case TOK_CARET:
            return PREC_POWER;

        default:
            return PREC_PRIMARY;
    }
}

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

static void vm_enter(VM *v)
{
    Scope *s = malloc(sizeof(Scope));
    da_init(s);
    da_append(v->env, s);
}

static void vm_leave(VM *v)
{
    Scope *s = da_last(v->env);
    scope_free(s);
    v->env->len--;
}

// Initialize a VM with default base;
void vm_init(VM *v)
{
    v->base = BASE_DEFAULT;
    v->ta = NULL;
    v->pos = 0;
    v->env = malloc(sizeof(Env));
    da_init(v->env);
    vm_enter(v);
}

void vm_reset(VM *v)
{
    v->base = BASE_DEFAULT;
    v->ta = NULL;
    v->pos = 0;
    for (size_t i = 1; i > v->env->len; i++)
    {
        scope_free(da_at(v->env, i));
    }
    da_reset(v->env);
}

// Free a VM.
void vm_free(VM *v)
{
    if (v->ta)
        da_free(v->ta);
    v->pos = 0;
    v->base = 0;
    DA_FOR(v->env, i)
    {
        scope_free(da_at(v->env, i));
    }
    da_free(v->env);
    free(v->env);
}

#define AT_OR_LAST(v, p) (p) < (v)->ta->len ? (p) : (v)->ta->len-1

// Peek the next n token.
static Token vm_peek(const VM *v, size_t n)
{
    return v->ta->data[AT_OR_LAST(v, v->pos+n)];
}

// Get the current token.
static Token vm_token(const VM *v)
{
    return vm_peek(v, 0);
}

// Get the current precedence.
static int vm_prec(const VM *v)
{
    return token_get_prec(vm_token(v));
}

// Check if the current expression is an alphanumeric literal.
static bool vm_is_alnum(const VM *v)
{
    Token t = vm_token(v);
    return t.kind == TOK_ALNUM || t.kind == TOK_DIGIT;
}

// Check if the current expression is a digit list.
static bool vm_is_digit_list(const VM *v)
{
    Token t = vm_token(v);
    return t.kind == TOK_LBRAC;
}

// Check if the current expression is considered singular.
static bool vm_is_single_expr(const VM *v)
{
    return vm_is_alnum(v) || vm_is_digit_list(v)
        || vm_token(v).kind == TOK_LPAREN;
}

#define CONSUME_EXPECT(v, tk, err) do { \
    Token t = vm_token(v); \
    if (t.kind != tk) \
        return value_errorf(err, t.pos, "Expected '%s'", tk_to_str[tk]); \
    (v)->pos++; \
} while (0)

static bool eval_expr(VM *v, int prec, Value *out);

// Convert a token to an unsigned long value.
// t must be TOK_DIGIT.
static bool token_to_ul(Token t, unsigned long *out)
{
    SV_TO_CSTR(t.value, buffer);

    errno = 0;

    char *end_ptr = NULL;
    *out = strtoul(buffer, &end_ptr, 10);
    if (errno == ERANGE)
        return false;

    return true;
}

// Parse an alphanumeric number part.
static bool parse_number_part_alnum(VM *v, DigitArray *ds, Value *err)
{
    Token t = vm_token(v);
    if (!vm_is_alnum(v))
        return value_errorf(err, t.pos, "Expected alphanumerics");

    DigitResult res = digits_from_alnum(ds, t.value, v->base);
    switch (res.kind)
    {
        case DIGIT_INVALID:
            return value_errorf(err, t.pos+res.pos, "Not a digit");
        case DIGIT_OOB:
            return value_errorf(err, t.pos+res.pos, "Digit out of bounds for base %lu", v->base);
        case DIGIT_BASE_TOO_LARGE:
            return value_errorf(err, t.pos, "Base too large for alphanumeric spelling.");
        default:
            break;
    }

    v->pos++;
    return true;
}

static bool parse_number_part_list(VM *v, DigitArray *ds, Value *err)
{
    Token t = vm_token(v);
    if (!vm_is_digit_list(v))
        return value_errorf(err, t.pos, "Expected digit list");

    CONSUME_EXPECT(v, TOK_LBRAC, err);
    for (;;)
    {
        t = vm_token(v);
        if (t.kind != TOK_DIGIT)
            return value_errorf(err, t.pos, "Expected numeric value as digit");

        unsigned long val;
        if (!token_to_ul(t, &val) || val >= v->base)
            return value_errorf(err, t.pos, "Digit out of bounds");
        v->pos++;

        da_append(ds, val);

        if (vm_token(v).kind == TOK_RBRAC) break;
        CONSUME_EXPECT(v, TOK_COMMA, err);
    }
    CONSUME_EXPECT(v, TOK_RBRAC, err);

    return true;
}

// Parse a number part (I, N, or R) into a sequence of digits.
// Ensure the number is in the same format as fmt.
static bool parse_number_part(VM *v, DigitArray *ds, DigitFormat fmt, Value *err)
{
    switch (fmt)
    {
        case DIGIT_FMT_ALNUM:
            return parse_number_part_alnum(v, ds, err);

        case DIGIT_FMT_LIST:
            return parse_number_part_list(v, ds, err);

        default:
            return value_errorf(err, vm_token(v).pos, "Expected number");
    }
}

// Evaluate a number literal.
static bool eval_number(VM *v, DigitFormat fmt, Value *out)
{
    bool ok = true;

    Literal lit;
    literal_init(&lit);
    if (!(ok = parse_number_part(v, &lit.I, fmt, out)))
        goto cleanup;

    if (vm_token(v).kind != TOK_DOT)
        goto eval;
    v->pos++;

    if (vm_token(v).kind != TOK_LPAREN)
    {
        if (!(ok = parse_number_part(v, &lit.N, fmt, out)))
            goto cleanup;
    }

    if (vm_token(v).kind == TOK_LPAREN)
    {
        v->pos++;
        if (!(ok = parse_number_part(v, &lit.R, fmt, out)))
            goto cleanup;
        CONSUME_EXPECT(v, TOK_RPAREN, out);
    }

eval:
    value_number(out);
    literal_to_mpq(&lit, v->base, out->as.number);

cleanup:
    literal_free(&lit);
    return ok;
}

// Evaluate a base annotation prefix.
static bool eval_base(VM *v, Value *out)
{
    Token t = vm_token(v);
    CONSUME_EXPECT(v, TOK_DIGIT, out);
    CONSUME_EXPECT(v, TOK_HASH, out);

    unsigned long prev_base = v->base;

    if (!token_to_ul(t, &v->base))
        return value_errorf(out, t.pos, "Base too large");
    if (v->base <= 1)
        return value_errorf(out, t.pos, "Base must be at least 2");

    if (!vm_is_single_expr(v))
        return value_errorf(out, vm_token(v).pos, "Expected expression");

    bool ok = eval_expr(v, PREC_BASE, out);
    v->base = prev_base;

    return ok;
}

// Evaluate a group expression.
static bool eval_group(VM *v, Value *out)
{
    CONSUME_EXPECT(v, TOK_LPAREN, out);
        if (!eval_expr(v, PREC_PRIMARY, out))
            return false;
    CONSUME_EXPECT(v, TOK_RPAREN, out);
    return true;
}

// Evaluate a negation expression.
static bool eval_neg(VM *v, Value *out)
{
    CONSUME_EXPECT(v, TOK_MINUS, out);

    Token t = vm_token(v);
    bool num_or_group = t.kind == TOK_LPAREN || vm_is_alnum(v) 
        || vm_is_digit_list(v);
    if (!num_or_group)
        return value_errorf(out, t.pos, "Expected number or group");

    if (!eval_expr(v, PREC_PREFIX, out))
        return false;

    if (out->kind != VAL_NUMBER)
        return false;

    mpq_neg(out->as.number, out->as.number);
    return true;
}

// Assign a symbol to the current scope.
static void vm_symbol_set(VM *v, StringView id, const Value *value)
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

static bool vm_symbol_get(VM *v, StringView id, Value *out)
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

// Evaluate an assignment expression.
static bool eval_assign(VM *v, Value *out)
{
    Token id = vm_token(v);
    CONSUME_EXPECT(v, TOK_ID, out);
    CONSUME_EXPECT(v, TOK_ASSIGN, out);
    if (!eval_expr(v, PREC_PRIMARY, out))
        return false;
    vm_symbol_set(v, id.value, out);
    return true;
}

// Evaluate a symbol.
static bool eval_symbol(VM *v, Value *out)
{
    Token id = vm_token(v);
    v->pos++;
    if (!vm_symbol_get(v, id.value, out))
        return value_errorf(out, id.pos, "Not found in the current scope");
    return true;
}

// Evaluate a builtin constant, return false if not a builtin.
static bool try_eval_builtin(VM *v, Value *out)
{
    Token t = vm_token(v);

    if (t.kind == TOK_TRUE)
        value_bool(out, true);

    else if (t.kind == TOK_FALSE)
        value_bool(out, false);

    else
        return false;

    v->pos++;
    return true;
}

// Evaluate a null denotation expression.
static bool eval_nud(VM *v, Value *out)
{
    Token t = vm_token(v);
    if (t.kind == TOK_EOF)
        return value_errorf(out, t.pos, "Expected expression");

    if (vm_peek(v, 1).kind == TOK_HASH)
        return eval_base(v, out);

    if (vm_peek(v, 1).kind == TOK_ASSIGN)
        return eval_assign(v, out);

    if (t.kind == TOK_ID)
        return eval_symbol(v, out);

    if (vm_is_alnum(v))
        return eval_number(v, DIGIT_FMT_ALNUM, out);

    if (vm_is_digit_list(v))
        return eval_number(v, DIGIT_FMT_LIST, out);

    if (t.kind == TOK_LPAREN)
        return eval_group(v, out);

    if (t.kind == TOK_MINUS)
        return eval_neg(v, out);

    if (try_eval_builtin(v, out))
        return out->kind != VAL_ERROR;

    return value_errorf(out, t.pos, "Expected expression");
}

#define MPQ_INFIX(f, l, r) f((l)->as.number, (l)->as.number, (r)->as.number)

#define MPQ_CMP(l, r) mpq_cmp((l)->as.number, (r)->as.number)

static bool eval_number_power(Value *l, Value *r, size_t r_pos)
{
    if (mpz_cmp_ui(mpq_denref(r->as.number), 1) == 0)
    {
        if (!mpz_fits_ulong_p(mpq_numref(r->as.number)))
            return value_errorf(l, r_pos, "Exponent too large");

        unsigned long exp = mpz_get_ui(mpq_numref(r->as.number));
        mpz_pow_ui(mpq_numref(l->as.number), mpq_numref(l->as.number), exp);
        mpz_pow_ui(mpq_denref(l->as.number), mpq_denref(l->as.number), exp);
        mpq_canonicalize(l->as.number);
    }
    else
    {
        return value_errorf(l, r_pos, "Non-integer exponent is not supported");
    }
    return true;
}

// Evaluate infix operation between numbers.
static bool eval_number_infix(Value *l, Token op, Value *r, size_t r_pos)
{
    switch (op.kind)
    {
        case TOK_PLUS:
            MPQ_INFIX(mpq_add, l, r);
            break;

        case TOK_MINUS:
            MPQ_INFIX(mpq_sub, l, r);
            break;

        case TOK_STAR:
            MPQ_INFIX(mpq_mul, l, r);
            break;

        case TOK_SLASH:
            if (mpq_cmp_ui(r->as.number, 0, 1) == 0)
                return value_errorf(l, r_pos, "Division by zero");
            MPQ_INFIX(mpq_div, l, r);
            break;

        case TOK_CARET:
            return eval_number_power(l, r, r_pos);

        case TOK_LT:
            value_bool(l, MPQ_CMP(l, r) < 0);
            break;

        case TOK_LEQ:
            value_bool(l, MPQ_CMP(l, r) <= 0);
            break;

        case TOK_GT:
            value_bool(l, MPQ_CMP(l, r) > 0);
            break;

        case TOK_GEQ:
            value_bool(l, MPQ_CMP(l, r) >= 0);
            break;

        case TOK_EQ:
            value_bool(l, MPQ_CMP(l, r) == 0);
            break;

        case TOK_NEQ:
            value_bool(l, MPQ_CMP(l, r) != 0);
            break;

        default:
            return value_errorf(l, op.pos, "Unknown operator");
    }
    return true;
}

// Evaluate boolean infix operations.
static bool eval_bool_infix(Value *l, Token op, Value *r)
{
    switch (op.kind)
    {
        case TOK_EQ:
            value_bool(l, l->as.boolean == r->as.boolean);
            break;

        case TOK_NEQ:
            value_bool(l, l->as.boolean != r->as.boolean);
            break;

        default:
            return value_errorf(l, op.pos, "Unknown operator");
    }
    return true;
}

// Evaluate a left denotation expression.
static bool eval_led(VM *v, int prec, Value *left)
{
    bool ok = true;

    Token infix = vm_token(v);
    v->pos++;

    size_t r_tok_pos = v->pos;
    Value right = {0};
    if (!eval_expr(v, prec, &right))
    {
        value_set(left, &right);
        ok = false;
        goto cleanup;
    }

    if (left->kind != right.kind)
    {
        ok = value_errorf(left, infix.pos, 
                "Invalid operation between %s and %s", 
                vk_to_str[left->kind], vk_to_str[right.kind]);
        goto cleanup;
    }

    switch (left->kind)
    {
        case VAL_ERROR:
            ok = false; 
            break;
        case VAL_NUMBER:
            ok = eval_number_infix(left, infix, &right, r_tok_pos);
            break;
        case VAL_BOOL:
            ok = eval_bool_infix(left, infix, &right);
            break;
        default:
            ok = value_errorf(left, infix.pos, "Unknown operation");
            break;
    }

cleanup:
    vm_value_free(&right);
    return ok;
}

// Evaluate an expression.
static bool eval_expr(VM *v, int prec, Value *out)
{
    if (!eval_nud(v, out))
        return false;

    for (int p = vm_prec(v); p > prec; p = vm_prec(v))
    {
        if (vm_token(v).kind == TOK_EOF) break;
        if (!eval_led(v, p, out))
            return false;
    }

    return true;
}

// Evaluate a source expression. out value will be reset but not freed before doing so.
bool vm_evaluate(VM *v, StringView src, Value *out)
{
    assert(v && out);
    v->pos = 0;
    out->kind = VAL_VOID;

    TokenArray ta = {0};
    if (!tokenize(&ta, src))
        return value_errorf(out, ta.data[0].pos, "Invalid token");
    v->ta = &ta;

    if (!eval_expr(v, PREC_PRIMARY, out) || out->kind == VAL_ERROR)
        return false;

    if (vm_token(v).kind != TOK_EOF)
        return value_errorf(out, vm_token(v).pos, "Expected operator");

    return true;
}

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
        for (size_t i = 0; i < v->as.error.pos; i++)
            str_append(sb, " ");

        if (ctx->use_color) str_appendf(sb, AFMT_BOLD ACOLOR_MAGENTA);
        str_appendf(sb, "^ "SV_FMT, SV_ARG(SV(v->as.error.msg)));
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
