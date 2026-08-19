#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "vm.h"
#include "digit.h"

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
            if (v->as.error.msg)
                free(v->as.error.msg);
            break;

        default:
            break;
    }
}

// Set a value from another value.
static void value_set(Value *v, Value *from)
{
    v->kind = from->kind;
    switch (v->kind)
    {
        case VAL_VOID:
            break;

        case VAL_NUMBER:
            mpq_set(v->as.number, from->as.number);
            break;

        case VAL_BOOL:
            v->as.boolean = from->as.boolean;
            break;

        case VAL_ERROR:
            v->as.error.pos = from->as.error.pos;
            v->as.error.msg = strdup(from->as.error.msg);
            break;
    }
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

    va_list args, copy;
    va_start(args, fmt);
    va_copy(copy, args);

    int len = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if (len > 0)
    {
        v->as.error.msg = malloc(len+1);
        assert(v->as.error.msg);

        vsnprintf(v->as.error.msg, len+1, fmt, args);
    }

    va_end(args);
    return false;
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

// Initialize a VM with default base;
void vm_init(VM *v)
{
    v->base = BASE_DEFAULT;
    v->ta = NULL;
    v->pos = 0;
}

// Free a VM.
void vm_free(VM *v)
{
    if (v->ta)
        ta_free(v->ta);
    v->pos = 0;
    v->base = 0;
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

// Find the next token of the given kind and return the offset from the current position.
static size_t vm_find_next(VM *v, TokenKind tk)
{
    size_t prev_pos = v->pos;
    while (vm_token(v).kind != tk)
    {
        v->pos++;
        if (vm_token(v).kind == TOK_EOF)
            return SIZE_MAX;
    }
    size_t d = v->pos - prev_pos;
    v->pos = prev_pos;
    return d;
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
    char buffer[t.len+1];
    memcpy(buffer, t.data, t.len);
    buffer[t.len] = '\0';

    errno = 0;

    char *end_ptr = NULL;
    *out = strtoul(buffer, &end_ptr, 10);
    if (errno == ERANGE)
        return false;

    return true;
}

// Parse an alphanumeric number part.
static bool parse_number_part_alnum(VM *v, Digits *ds, Value *err)
{
    Token t = vm_token(v);
    if (!vm_is_alnum(v))
        return value_errorf(err, t.pos, "Expected alphanumerics");

    DigitResult res = digits_from_alnum(ds, t.data, t.len, v->base);
    switch (res.kind)
    {
        case DIGIT_INVALID:
            return value_errorf(err, t.pos+res.pos, "Not a digit");
        case DIGIT_OOB:
            return value_errorf(err, t.pos+res.pos, "Digit out of bounds for base %lu", v->base);
        default:
            break;
    }

    v->pos++;
    return true;
}

static bool parse_number_part_list(VM *v, Digits *ds, Value *err)
{
    Token t = vm_token(v);
    if (!vm_is_digit_list(v))
        return value_errorf(err, t.pos, "Expected digit list");

    CONSUME_EXPECT(v, TOK_LBRAC, err);
    size_t i = 0;
    size_t len = vm_find_next(v, TOK_RBRAC);
    if (len == SIZE_MAX)
        return value_errorf(err, vm_token(v).pos, "Expected ']");

    digits_alloc(ds, len);
    for (;;)
    {
        t = vm_token(v);
        if (t.kind != TOK_DIGIT)
            return value_errorf(err, t.pos, "Expected numeric value as digit");

        unsigned long val;
        if (!token_to_ul(t, &val) || val >= v->base)
            return value_errorf(err, t.pos, "Digit out of bounds");
        v->pos++;

        ds->data[i++] = val;
        if (vm_token(v).kind == TOK_RBRAC) break;
        CONSUME_EXPECT(v, TOK_COMMA, err);
    }
    ds->len = i;
    CONSUME_EXPECT(v, TOK_RBRAC, err);

    return true;
}

// Parse a number part (I, N, or R) into a sequence of digits.
// Ensure the number is in the same format as fmt.
static bool parse_number_part(VM *v, Digits *ds, DigitFormat fmt, Value *err)
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

// Evaluate a null denotation expression.
static bool eval_nud(VM *v, Value *out)
{
    Token t = vm_token(v);
    if (t.kind == TOK_EOF)
        return value_errorf(out, t.pos, "Expected expression");

    if (vm_peek(v, 1).kind == TOK_HASH)
        return eval_base(v, out);

    else if (vm_is_alnum(v))
        return eval_number(v, DIGIT_FMT_ALNUM, out);

    else if (vm_is_digit_list(v))
        return eval_number(v, DIGIT_FMT_LIST, out);

    else if (t.kind == TOK_LPAREN)
        return eval_group(v, out);

    else if (t.kind == TOK_MINUS)
        return eval_neg(v, out);

    else
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

// Evaluate a left denotation expression.
static bool eval_led(VM *v, int prec, Value *left)
{
    Token t = vm_token(v);
    v->pos++;

    size_t r_tok_pos = v->pos;
    Value right = {0};
    if (!eval_expr(v, prec, &right))
    {
        value_set(left, &right);
        vm_value_free(&right);
        return false;
    }

    if (left->kind != right.kind)
        return value_errorf(left, t.pos, 
                "Invalid operation between %s and %s", 
                vk_to_str[left->kind], vk_to_str[right.kind]);

    switch (left->kind)
    {
        case VAL_VOID:    assert(!"void value reached eval_led");
        case VAL_ERROR:   return false;
        case VAL_NUMBER:  return eval_number_infix(left, t, &right, r_tok_pos);
        case VAL_BOOL:    return value_errorf(left, t.pos, "bool is not implemented");
    }

    vm_value_free(&right);
    return true;
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
bool vm_evaluate(VM *v, const char *src, Value *out)
{
    assert(v && out);
    v->pos = 0;
    out->kind = VAL_VOID;

    TokenArray ta = {0};
    if (!tokenize(&ta, src))
        return value_errorf(out, ta.data[0].pos, "Invalid token");
    v->ta = &ta;

    bool ok = eval_expr(v, PREC_PRIMARY, out);
    if (vm_token(v).kind != TOK_EOF)
        return value_errorf(out, vm_token(v).pos, "Expected operator");

    return ok;
}

// Print the value to stdout.
// The src string must be the one that produced the value.
void vm_value_print(Value *v, const char *src)
{
    switch (v->kind)
    {
        case VAL_VOID:
            printf("<void>\n");
            break;

        case VAL_ERROR:
            printf("%s\n", src);
            for (size_t i = 0; i < v->as.error.pos; i++)
                printf(" ");
            printf("^ %s\n", v->as.error.msg);
            break;

        case VAL_NUMBER:
            gmp_printf("%Qd\n", v->as.number);
            break;

        case VAL_BOOL:
            printf("%s\n", v->as.boolean ? "true" : "false");
            break;
    }
}
