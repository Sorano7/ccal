#ifndef CREAL_H
#define CREAL_H

#include <gmp.h>
#include <mpfr.h>
#include <mpfi.h>

#define PREC_DEFAULT 10

typedef struct CRNode CRNode;

CRNode *cr_from_mpq(const mpq_t q);

CRNode *cr_sqrt(CRNode *a);

CRNode *cr_copy(CRNode *from);

void cr_free(CRNode *n);

void cr_compute(CRNode *n, mp_prec_t target_prec, mpfi_t result);

#endif
