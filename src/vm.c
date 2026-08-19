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

// Operator precedence levels.
typedef enum
{
    PREC_PRIMARY,
    PREC_SUM,
    PREC_PRODUCT,
    PREC_PREFIX,
    PREC_BASE,
} OpPrec;

// Get the precedence of the token.
static OpPrec token_get_prec(Token t)
{
    switch (t.kind)
    {
        case TOK_PLUS:
        case TOK_MINUS:
            return PREC_SUM;

        case TOK_STAR:
        case TOK_SLASH:
            return PREC_PRODUCT;

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

// Create a formatted error result pointing to the position.
static VMResult errorf_at(size_t pos, const char *fmt, ...)
{
    VMResult r = {.ok=false, .span_start=pos, .msg=NULL};

    va_list args, copy;
    va_start(args, fmt);
    va_copy(copy, args);

    int len = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if (len > 0)
    {
        r.msg = malloc(len+1);
        assert(r.msg);

        vsnprintf(r.msg, len+1, fmt, args);
    }

    va_end(args);

    return r;
}

#define errorf_tok(tok, fmt, ...) errorf_at((tok.pos), \
        (fmt) __VA_OPT__(,) __VA_ARGS__)

#define EVAL_OK (VMResult){.ok=true, .span_start=0, .msg=0}

#define CONSUME_EXPECT(v, tk) do { \
    if (vm_token(v).kind != tk) \
        return errorf_tok(vm_token(v), "Expected '%s'", tk_to_str[tk]); \
    (v)->pos++; \
} while (0)

#define ENSURE_OK(r) do { \
    VMResult __tmp = (r); \
    if (!__tmp.ok) return __tmp; \
} while (0)

static VMResult eval_expr(VM *v, int prec, mpq_t out);

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
static VMResult parse_number_part_alnum(VM *v, Digits *ds)
{
    Token t = vm_token(v);
    if (!vm_is_alnum(v))
        return errorf_tok(t, "Expected alphanumerics");

    DigitResult res = digits_from_alnum(ds, t.data, t.len, v->base);
    switch (res.kind)
    {
        case DIGIT_INVALID:
            return errorf_at(t.pos+res.pos, "Not a digit");
        case DIGIT_OOB:
            return errorf_at(t.pos+res.pos, "Digit out of bounds for base %lu", v->base);
        default:
            break;
    }

    v->pos++;
    return EVAL_OK;
}

static VMResult parse_number_part_list(VM *v, Digits *ds)
{
    Token t = vm_token(v);
    if (!vm_is_digit_list(v))
        return errorf_tok(t, "Expected digit list");

    CONSUME_EXPECT(v, TOK_LBRAC);
    size_t i = 0;
    size_t len = vm_find_next(v, TOK_RBRAC);
    if (len == SIZE_MAX)
        return errorf_tok(vm_token(v), "Expected ']");

    digits_alloc(ds, len);
    for (;;)
    {
        t = vm_token(v);
        if (t.kind != TOK_DIGIT)
            return errorf_tok(t, "Expected numeric value as digit");

        unsigned long val;
        if (!token_to_ul(t, &val) || val >= v->base)
            return errorf_tok(t, "Digit out of bounds");
        v->pos++;

        ds->data[i++] = val;
        if (vm_token(v).kind == TOK_RBRAC) break;
        CONSUME_EXPECT(v, TOK_COMMA);
    }
    ds->len = i;
    CONSUME_EXPECT(v, TOK_RBRAC);

    return EVAL_OK;
}

// Parse a number part (I, N, or R) into a sequence of digits.
// Ensure the number is in the same format as fmt.
static VMResult parse_number_part(VM *v, Digits *ds, DigitFormat fmt)
{
    switch (fmt)
    {
        case DIGIT_FMT_ALNUM:
            return parse_number_part_alnum(v, ds);

        case DIGIT_FMT_LIST:
            return parse_number_part_list(v, ds);

        default:
            return errorf_tok(vm_token(v), "Expected number");
    }
}

// Evaluate a number literal.
static VMResult eval_number(VM *v, DigitFormat fmt, mpq_t out)
{
    VMResult res = EVAL_OK;

    Literal lit;
    literal_init(&lit);
    ENSURE_OK((res = parse_number_part(v, &lit.I, fmt)));

    if (vm_token(v).kind != TOK_DOT)
        goto eval;
    v->pos++;

    if (vm_token(v).kind != TOK_LPAREN)
    {
        res = parse_number_part(v, &lit.N, fmt);
        if (!res.ok) goto cleanup;
    }

    if (vm_token(v).kind == TOK_LPAREN)
    {
        v->pos++;
        res = parse_number_part(v, &lit.R, fmt);
        if (!res.ok) goto cleanup;
        CONSUME_EXPECT(v, TOK_RPAREN);
    }

eval:
    literal_to_mpq(&lit, v->base, out);

cleanup:
    literal_free(&lit);
    return res;
}

// Evaluate a base annotation prefix.
static VMResult eval_base(VM *v, mpq_t out)
{
    Token t = vm_token(v);
    CONSUME_EXPECT(v, TOK_DIGIT);
    CONSUME_EXPECT(v, TOK_HASH);

    unsigned long prev_base = v->base;

    if (!token_to_ul(t, &v->base))
        return errorf_tok(t, "Base too large");
    if (v->base <= 1)
        return errorf_tok(t, "Base must be at least 2");

    VMResult r = eval_expr(v, PREC_BASE, out);
    v->base = prev_base;

    return r;
}

// Evaluate a group expression.
static VMResult eval_group(VM *v, mpq_t out)
{
    CONSUME_EXPECT(v, TOK_LPAREN);
        ENSURE_OK(eval_expr(v, PREC_PRIMARY, out));
    CONSUME_EXPECT(v, TOK_RPAREN);
    return EVAL_OK;
}

// Evaluate a negation expression.
static VMResult eval_neg(VM *v, mpq_t out)
{
    CONSUME_EXPECT(v, TOK_MINUS);

    Token t = vm_token(v);
    bool num_or_group = t.kind == TOK_LPAREN || vm_is_alnum(v) || vm_is_digit_list(v);
    if (!num_or_group)
        return errorf_tok(t, "Expected number or group");

    ENSURE_OK(eval_expr(v, PREC_PREFIX, out));
    mpq_neg(out, out);
    return EVAL_OK;
}

// Evaluate a null denotation expression.
static VMResult eval_nud(VM *v, mpq_t out)
{
    if (vm_peek(v, 1).kind == TOK_HASH)
        return eval_base(v, out);

    else if (vm_is_alnum(v))
        return eval_number(v, DIGIT_FMT_ALNUM, out);

    else if (vm_is_digit_list(v))
        return eval_number(v, DIGIT_FMT_LIST, out);

    Token t = vm_token(v);
    switch (t.kind)
    {
        case TOK_LPAREN: return eval_group(v, out);
        case TOK_MINUS:  return eval_neg(v, out);

        case TOK_EOF:
        default:         return errorf_tok(t, "Expected expression");
    }
}

// Evaluate a left denotation expression.
static VMResult eval_led(VM *v, int prec, mpq_t left)
{
    Token t = vm_token(v);
    v->pos++;

    mpq_t right;
    mpq_init(right);
    ENSURE_OK(eval_expr(v, prec, right));

    switch (t.kind)
    {
        case TOK_PLUS:
            mpq_add(left, left, right);
            break;

        case TOK_MINUS:
            mpq_sub(left, left, right);
            break;

        case TOK_STAR:
            mpq_mul(left, left, right);
            break;

        case TOK_SLASH:
            if (mpq_cmp_ui(right, 0, 1) == 0)
                return errorf_tok(t, "Division by zero");
            mpq_div(left, left, right);
            break;

        default:
            return errorf_tok(t, "Expected infix operator");
    }

    mpq_clear(right);
    return EVAL_OK;
}

// Evaluate an expression.
static VMResult eval_expr(VM *v, int prec, mpq_t out)
{
    mpq_t left;
    mpq_init(left);

    ENSURE_OK(eval_nud(v, left));

    for (int p = vm_prec(v); p > prec; p = vm_prec(v))
    {
        if (vm_token(v).kind == TOK_EOF) break;
        ENSURE_OK(eval_led(v, p, left));
    }

    mpq_set(out, left);
    mpq_clear(left);
    return EVAL_OK;
}

// Evaluate a source expression. Can be called repeatedly without manual reset.
// The returned VMResult must be freed.
VMResult vm_evaluate(VM *v, const char *src, mpq_t out)
{
    assert(v);
    v->pos = 0;

    TokenArray ta = {0};
    assert(tokenize(&ta, src));
    v->ta = &ta;

    mpq_set_ui(out, 0, 1);
    return eval_expr(v, PREC_PRIMARY, out);
}

// Prints diagnostic message to stdout.
// The src string must be the one that produced the result.
void vm_diagnostics(VMResult *r, const char *src)
{
    assert(r && src);
    if (r->ok) return;

    printf("%s\n", src);
    for (size_t i = 0; i < r->span_start; i++)
        printf(" ");
    printf("^ %s\n", r->msg);
}

// Free a VMResult.
void vm_result_free(VMResult *r)
{
    if (!r) return;
    if (r->msg) free(r->msg);
    r->span_start = 0;
    r->ok = 0;
}

