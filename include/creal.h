#ifndef CREAL_H
#define CREAL_H

#include <gmp.h>
#include <mpfr.h>
#include <mpfi.h>

#define PREC_DEFAULT 10

typedef struct CRNode CR;

CR *cr_from_mpq(const mpq_t q);

CR *cr_pi(void);
CR *cr_e(void);

CR *cr_add(CR *a, CR *b);
CR *cr_sub(CR *a, CR *b);
CR *cr_mul(CR *a, CR *b);
CR *cr_div(CR *a, CR *b);

CR *cr_neg(CR *a);
CR *cr_sqrt(CR *a);

CR *cr_copy(CR *from);

void cr_free(CR *n);

void cr_eval(CR *n, mp_prec_t target_prec, mpfi_t result);

#endif
