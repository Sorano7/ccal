#include "creal.h"
#include "number.h"
#include "cut.h"
#include <stdlib.h>
#include <math.h>

#define GUARD_BITS 32
#define MAX_PREC_BITS (1u << 20)

#define APPROX_TOL 1e-12
#define APPROX_D(a, b) (fabs(a - b) < APPROX_TOL)

typedef enum
{
    CR_ERROR,
    CR_LEAF_RATIONAL,
    CR_LEAF_PI,
    CR_LEAF_E,

    CR_OP_ADD,
    CR_OP_SUB,
    CR_OP_MUL,
    CR_OP_DIV,
    CR_OP_MOD,

    CR_OP_NEG,
    CR_OP_SQRT,

    CR_OP_EXP,
    CR_OP_LN,
} CRKind;

typedef struct CRNode
{
    union
    {
        struct
        {
            struct CRNode *l;
            struct CRNode *r;

            mpq_t rational;

            mpfi_t cached_interval;
            mp_prec_t cached_prec;
            bool has_cache;
        } node;

        String error;
    };
    size_t refcount;
    CRKind kind;
} CR;

static int cr_sign(CR *n)
{
    double d = cr_to_d(n);
    if (APPROX_D(d, 0))
        return 0;
    return d > 0 ? 1 : -1;
}

// Allocate a new CR node.
static CR *cr_new(CRKind kind)
{
    CR *n = calloc(1, sizeof(CR));
    n->kind = kind;
    n->refcount = 1;
    n->node.has_cache = false;
    return n;
}

static CR *cr_error(const char *fmt, ...)
{
    CR *n = calloc(1, sizeof(CR));
    n->kind = CR_ERROR;
    str_init(&n->error);

    va_list args;
    va_start(args, fmt);
    str_appendvf(&n->error, fmt, args);
    va_end(args);

    return n;
}

static CR *cr_new_binary(CRKind kind, CR *a, CR *b)
{
    CR *n = cr_new(kind);
    n->node.l = a; a->refcount++;
    n->node.r = b; b->refcount++;
    return n;
}

static CR *cr_new_unary(CRKind kind, CR *a)
{
    CR *n = cr_new(kind);
    n->node.l = a; a->refcount++;
    return n;
}

CR *cr_from_mpq(const mpq_t q)
{
    CR *n = cr_new(CR_LEAF_RATIONAL);
    mpq_init(n->node.rational);
    mpq_set(n->node.rational, q);
    return n;
}

CR *cr_from_si(long n, long d)
{
    mpq_t q;
    mpq_init(q);
    mpq_set_si(q, n, d > 0 ? (unsigned long)d : (unsigned long)-d);
    if (d < 0) mpq_neg(q, q);
    mpq_canonicalize(q);
    CR *c = cr_from_mpq(q);
    mpq_clear(q);
    return c;
}

CR *cr_pi(void)
{
    return cr_new(CR_LEAF_PI);
}

CR *cr_e(void)
{
    return cr_new(CR_LEAF_E);
}

CR *cr_add(CR *a, CR *b)
{
    return cr_new_binary(CR_OP_ADD, a, b);
}

CR *cr_sub(CR *a, CR *b)
{
    return cr_new_binary(CR_OP_SUB, a, b);
}

CR *cr_mul(CR *a, CR *b)
{
    return cr_new_binary(CR_OP_MUL, a, b);
}

CR *cr_div(CR *a, CR *b)
{
    if (cr_sign(b) == 0)
        return cr_error("Division by zero");
    return cr_new_binary(CR_OP_DIV, a, b);
}

CR *cr_mod(CR *a, CR *b)
{
    if (cr_sign(b) == 0)
        return cr_error("Modulo by zero");
    return cr_new_binary(CR_OP_MOD, a, b);
}

CR *cr_neg(CR *a)
{
    return cr_new_unary(CR_OP_NEG, a);
}

CR *cr_sqrt(CR *a) 
{
    if (cr_sign(a) < 0)
        return cr_error("Expected non-negative operand");
    return cr_new_unary(CR_OP_SQRT, a);
}

CR *cr_exp(CR *x)
{
    CR *e = cr_e();
    double base = cr_to_d(e);
    double exp = cr_to_d(x);
    double bits = bit_estimate(base, exp);
    cr_release(&e);

    if (mpfr_get_emax() < bits)
        return cr_error("Exponent too large");

    return cr_new_unary(CR_OP_EXP, x);
}

CR *cr_ln(CR *x) 
{
    if (cr_sign(x) <= 0)
        return cr_error("Expected non-negative/non-zero operand");
    return cr_new_unary(CR_OP_LN, x);
}

CR *cr_pow(CR *b, CR *x)
{
    double base = cr_to_d(b);
    double exp = cr_to_d(x);
    double bits = bit_estimate(base, exp);
    if (mpfr_get_emax() < bits)
        return cr_error("Exponent too large");

    CR *ln_b = cr_ln(b);
    if (cr_is_error(ln_b))
        return ln_b;

    CR *prod = cr_mul(x, ln_b);
    CR *n = cr_exp(prod);
    cr_release(&ln_b);
    cr_release(&prod);
    return n;
}

CR *cr_log(CR *b, CR *x)
{
    CR *ln_x = cr_ln(x);
    if (cr_is_error(ln_x))
        return ln_x;

    CR *ln_b = cr_ln(b);
    if (cr_is_error(ln_b))
    {
        cr_release(&ln_x);
        return ln_b;
    }

    CR *n = cr_div(ln_x, ln_b);
    cr_release(&ln_x);
    cr_release(&ln_b);
    return n;
}

// Shallow copies one node to another.
CR *cr_retain(CR *from)
{
    from->refcount++;
    return from;
}

// Free a CR node.
void cr_release(CR **np)
{
    if (!np || !*np) return;
    CR *n = *np;
    if (--n->refcount > 0) return;

    if (n->kind == CR_ERROR)
    {
        str_free(&n->error);
    }
    else
    {
        if (n->kind == CR_LEAF_RATIONAL)
            mpq_clear(n->node.rational);
        if (n->node.has_cache)
            mpfi_clear(n->node.cached_interval);

        cr_release(&n->node.l);
        cr_release(&n->node.r);
    }
    free(n);
    *np = NULL;
}

static void cr_rational_eval(CR *n, mp_prec_t p, mpfi_t out)
{
    mpfr_t lo, hi;
    mpfr_inits2(p, lo, hi, NULL);

    mpfr_set_q(lo, n->node.rational, MPFR_RNDD);
    mpfr_set_q(hi, n->node.rational, MPFR_RNDU);

    mpfi_interv_fr(out, lo, hi);
    mpfr_clears(lo, hi, NULL);
}

static void cr_pi_eval(mp_prec_t p, mpfi_t out)
{
    mpfi_set_prec(out, p);
    mpfi_const_pi(out);
}

static void cr_e_eval(mp_prec_t p, mpfi_t out)
{
    mpfr_t lo, hi;
    mpfr_inits2(p, lo, hi, NULL);

    mpfr_set_ui(lo, 1, MPFR_RNDD);
    mpfr_exp(lo, lo, MPFR_RNDD);
    mpfr_set_ui(hi, 1, MPFR_RNDU);
    mpfr_exp(hi, hi, MPFR_RNDU);

    mpfi_interv_fr(out, lo, hi);
    mpfr_clears(lo, hi, NULL);
}

static void cr_mod_eval(CR *n, mpfr_prec_t p, mpfi_t out)
{
    mpfi_t a, b;
    mpfi_inits2(p, a, b, NULL);
    cr_eval(n->node.l, p, a);
    cr_eval(n->node.r, p, b);

    mpfi_t q;
    mpfr_t qlo, qhi, nlo, nhi;

    mpfi_init2(q, p);
    mpfr_inits2(p, qlo, qhi, nlo, nhi, NULL);

    mpfi_div(q, a, b);
    mpfi_get_left(qlo, q);
    mpfi_get_right(qhi, q);
    mpfr_floor(nlo, qlo);
    mpfr_floor(nhi, qhi);

    if (mpfr_equal_p(nlo, nhi))
    {
        mpfi_t n, nb;
        mpfi_inits2(p, n, nb, NULL);
        mpfi_set_fr(n, nlo);
        mpfi_mul(nb, n, b);
        mpfi_sub(out, a, nb);
        mpfi_clears(n, nb, NULL);
    }
    else
    {
        mpfi_t absb;
        mpfr_t zero, sup;
        mpfi_init2(absb, p);
        mpfr_inits2(p, zero, sup, NULL);

        mpfi_abs(absb, b);
        mpfr_set_zero(zero, 1);
        mpfi_get_right(sup, absb);
        mpfi_interv_fr(out, zero, sup);

        mpfr_clears(zero, sup, NULL);
        mpfi_clear(absb);
    }

    mpfi_clear(q);
    mpfr_clears(qlo, qhi, nlo, nhi, NULL);
}

#define MPFI_BINARY(fn) do { \
    mpfi_t li, ri; \
    mpfi_inits2(p, li, ri, NULL); \
    cr_eval(n->node.l, p, li); \
    cr_eval(n->node.r, p, ri); \
    (fn)(out, li, ri); \
    mpfi_clears(li, ri, NULL); \
} while (0)

#define MPFI_UNARY(fn) do { \
    mpfi_t ci; \
    mpfi_init2(ci, p); \
    cr_eval(n->node.l, p, ci); \
    (fn)(out, ci); \
    mpfi_clear(ci); \
} while (0)

// Evaluate a CR to an interval enclosure.
void cr_eval(CR *n, mp_prec_t target_prec, mpfi_t result)
{
    if (n->node.has_cache && n->node.cached_prec >= target_prec)
    {
        mpfi_set(result, n->node.cached_interval);
        return;
    }

    mp_prec_t p = target_prec + GUARD_BITS;
    if (n->node.has_cache && n->node.cached_prec * 2 > p)
        p = n->node.cached_prec * 2;

    mpfi_t out;
    mpfi_init2(out, p);

    for (;;)
    {
        switch (n->kind)
        {
            case CR_LEAF_RATIONAL: cr_rational_eval(n, p, out); break;
            case CR_LEAF_PI:       cr_pi_eval(p, out);          break;
            case CR_LEAF_E:        cr_e_eval(p, out);           break;

            case CR_OP_ADD:        MPFI_BINARY(mpfi_add);       break;
            case CR_OP_SUB:        MPFI_BINARY(mpfi_sub);       break;
            case CR_OP_MUL:        MPFI_BINARY(mpfi_mul);       break;
            case CR_OP_DIV:        MPFI_BINARY(mpfi_div);       break;

            case CR_OP_MOD:        cr_mod_eval(n, p, out);      break;

            case CR_OP_NEG:        MPFI_UNARY(mpfi_neg);        break;
            case CR_OP_SQRT:       MPFI_UNARY(mpfi_sqrt);       break;
            case CR_OP_EXP:        MPFI_UNARY(mpfi_exp);        break;
            case CR_OP_LN:         MPFI_UNARY(mpfi_log);        break;

            default:               UNREACHABLE();
        }

        mpfr_t width, tol;
        mpfr_inits2(64, width, tol, NULL);
        mpfi_diam_abs(width, out);

        mpfr_set_ui_2exp(tol, 1, -(mpfr_exp_t)target_prec, MPFR_RNDN);
        bool good_enough = mpfr_cmp(width, tol) <= 0;
        mpfr_clears(width, tol, (mpfr_ptr)0);

        if (good_enough || p >= MAX_PREC_BITS) break;

        p *= 2;
        mpfi_set_prec(out, p);
    }

    if (n->node.has_cache)
        mpfi_clear(n->node.cached_interval);

    mpfi_init2(n->node.cached_interval, p);
    mpfi_set(n->node.cached_interval, out);

    n->node.cached_prec = p;
    n->node.has_cache = true;

    mpfi_set(result, out);
    mpfi_clear(out);
}

double cr_to_d(CR *n)
{
    mpfi_t out;
    mpfi_init2(out, 50);
    cr_eval(n, 50, out);
    double d = mpfi_get_d(out);
    mpfi_clear(out);
    return d;
}

bool cr_approx(CR *a, CR *b)
{
    double da = cr_to_d(a);
    double db = cr_to_d(b);
    return APPROX_D(da, db);
}

bool cr_is_error(const CR *n)
{
    return n->kind == CR_ERROR;
}

void cr_get_error(const CR *n, String *sb)
{
    if (!cr_is_error(n)) return;
    str_append(sb, &n->error);
}
