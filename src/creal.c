#include "creal.h"
#include "cut.h"
#include <stdlib.h>

#define GUARD_BITS 32
#define MAX_PREC_BITS (1u << 20)

typedef enum
{
    CR_LEAF_RATIONAL,
    CR_LEAF_PI,
    CR_LEAF_E,

    CR_OP_ADD,
    CR_OP_SUB,
    CR_OP_MUL,
    CR_OP_DIV,

    CR_OP_NEG,
    CR_OP_SQRT,

    CR_OP_EXP,
    CR_OP_LN,
} CRKind;

typedef struct CRNode
{
    struct CRNode *l;
    struct CRNode *r;

    mpq_t rational;

    mpfi_t cached_interval;
    mp_prec_t cached_prec;
    bool has_cache;

    size_t refcount;

    CRKind kind;
} CR;

// Allocate a new CR node.
static CR *cr_new(CRKind kind)
{
    CR *n = calloc(1, sizeof(CR));
    n->kind = kind;
    n->refcount = 1;
    n->has_cache = false;
    return n;
}

static CR *cr_new_binary(CRKind kind, CR *a, CR *b)
{
    CR *n = cr_new(kind);
    n->l = a; a->refcount++;
    n->r = b; b->refcount++;
    return n;
}

static CR *cr_new_unary(CRKind kind, CR *a)
{
    CR *n = cr_new(kind);
    n->l = a; a->refcount++;
    return n;
}

CR *cr_from_mpq(const mpq_t q)
{
    CR *n = cr_new(CR_LEAF_RATIONAL);
    mpq_init(n->rational);
    mpq_set(n->rational, q);
    return n;
}

CR *cr_pi(void)          { return cr_new(CR_LEAF_PI); }
CR *cr_e(void)           { return cr_new(CR_LEAF_E); }

CR *cr_add(CR *a, CR *b) { return cr_new_binary(CR_OP_ADD, a, b); }
CR *cr_sub(CR *a, CR *b) { return cr_new_binary(CR_OP_SUB, a, b); }
CR *cr_mul(CR *a, CR *b) { return cr_new_binary(CR_OP_MUL, a, b); }
CR *cr_div(CR *a, CR *b) { return cr_new_binary(CR_OP_DIV, a, b); }

CR *cr_neg(CR *a)        { return cr_new_unary(CR_OP_NEG, a); }
CR *cr_sqrt(CR *a)       { return cr_new_unary(CR_OP_SQRT, a); }

CR *cr_exp(CR *x)        { return cr_new_unary(CR_OP_EXP, x); }
CR *cr_ln(CR *x)         { return cr_new_unary(CR_OP_LN, x); }

CR *cr_pow(CR *b, CR *x)
{
    CR *ln_b = cr_ln(b);
    CR *prod = cr_mul(x, ln_b);
    CR *n = cr_exp(prod);
    cr_release(ln_b);
    cr_release(prod);
    return n;
}

CR *cr_log(CR *b, CR *x)
{
    CR *ln_x = cr_ln(x);
    CR *ln_b = cr_ln(b);
    CR *n = cr_div(ln_x, ln_b);
    cr_release(ln_x);
    cr_release(ln_b);
    return n;
}

// Shallow copies one node to another.
CR *cr_retain(CR *from)
{
    from->refcount++;
    return from;
}

// Free a CR node.
void cr_release(CR *n)
{
    if (!n) return;
    if (--n->refcount > 0) return;

    if (n->kind == CR_LEAF_RATIONAL)
        mpq_clear(n->rational);
    if (n->has_cache)
        mpfi_clear(n->cached_interval);

    cr_release(n->l);
    cr_release(n->r);
    free(n);
}

static void cr_rational_eval(CR *n, mp_prec_t p, mpfi_t out)
{
    mpfr_t lo, hi;
    mpfr_inits2(p, lo, hi, NULL);

    mpfr_set_q(lo, n->rational, MPFR_RNDD);
    mpfr_set_q(hi, n->rational, MPFR_RNDU);

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

#define MPFI_BINARY(fn) do { \
    mpfi_t li, ri; \
    mpfi_inits2(p, li, ri, NULL); \
    cr_eval(n->l, p, li); \
    cr_eval(n->r, p, ri); \
    (fn)(out, li, ri); \
    mpfi_clears(li, ri, NULL); \
} while (0)

#define MPFI_UNARY(fn) do { \
    mpfi_t ci; \
    mpfi_init2(ci, p); \
    cr_eval(n->l, p, ci); \
    (fn)(out, ci); \
    mpfi_clear(ci); \
} while (0)

// Evaluate a CR to an interval enclosure.
void cr_eval(CR *n, mp_prec_t target_prec, mpfi_t result)
{
    if (n->has_cache && n->cached_prec >= target_prec)
    {
        mpfi_set(result, n->cached_interval);
        return;
    }

    mp_prec_t p = target_prec + GUARD_BITS;
    if (n->has_cache && n->cached_prec * 2 > p)
        p = n->cached_prec * 2;

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

    if (n->has_cache)
        mpfi_clear(n->cached_interval);

    mpfi_init2(n->cached_interval, p);
    mpfi_set(n->cached_interval, out);

    n->cached_prec = p;
    n->has_cache = true;

    mpfi_set(result, out);
    mpfi_clear(out);
}
