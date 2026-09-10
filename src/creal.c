#include "creal.h"
#include "cut.h"
#include <stdlib.h>

#define GUARD_BITS 32
#define MAX_PREC_BITS (1u << 20)

typedef enum
{
    CR_LEAF_RATIONAL,
    CR_OP_SQRT,
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
} CRNode;

// Allocate a new CReal node.
static CRNode *cr_new(CRKind kind)
{
    CRNode *n = calloc(1, sizeof(CRNode));
    n->kind = kind;
    n->refcount = 1;
    n->has_cache = false;
    return n;
}

// Create a CReal node from a rational.
CRNode *cr_from_mpq(const mpq_t q)
{
    CRNode *n = cr_new(CR_LEAF_RATIONAL);
    mpq_init(n->rational);
    mpq_set(n->rational, q);
    return n;
}

// Create a square root CReal node.
CRNode *cr_sqrt(CRNode *a)
{
    CRNode *n = cr_new(CR_OP_SQRT);
    n->l = a;
    a->refcount++;
    return n;
}

// Shallow copies one node to another.
CRNode *cr_copy(CRNode *from)
{
    from->refcount++;
    return from;
}

// Free a CReal node.
void cr_free(CRNode *n)
{
    if (!n) return;
    if (--n->refcount > 0) return;

    if (n->kind == CR_LEAF_RATIONAL)
        mpq_clear(n->rational);
    if (n->has_cache)
        mpfi_clear(n->cached_interval);

    cr_free(n->l);
    cr_free(n->r);
    free(n);
}

// Compute a rational node.
static void cr_rational_compute(CRNode *n, mp_prec_t p, mpfi_t out)
{
    mpfr_t lo, hi;
    mpfr_inits2(p, lo, hi, NULL);
    mpfr_set_q(lo, n->rational, MPFR_RNDD);
    mpfr_set_q(hi, n->rational, MPFR_RNDU);
    mpfi_interv_fr(out, lo, hi);
    mpfr_clears(lo, hi, NULL);
}

// Compute a squart root node.
static void cr_sqrt_compute(CRNode *n, mp_prec_t p, mpfi_t out)
{
    mpfi_t ci;
    mpfi_init2(ci, p);
    cr_compute(n->l, p, ci);
    mpfi_sqrt(out, ci);
    mpfi_clear(ci);
}

// Compute a CReal to an interval enclosure.
void cr_compute(CRNode *n, mp_prec_t target_prec, mpfi_t result)
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
            case CR_LEAF_RATIONAL: cr_rational_compute(n, p, out); break;
            case CR_OP_SQRT:       cr_sqrt_compute(n, p, out);     break;
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
    n->has_cache = 1;

    mpfi_set(result, out);
    mpfi_clear(out);
}
