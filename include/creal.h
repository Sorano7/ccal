#ifndef CREAL_H
#define CREAL_H

#include <gmp.h>
#include <mpfr.h>
#include <mpfi.h>

#define PREC_DEFAULT 10

typedef struct CRNode CR;

typedef CR *(*CRUnary)(CR *);
typedef CR *(*CRBinary)(CR *, CR *);

CR *cr_from_mpq(const mpq_t q);
CR *cr_from_si(long n, long d);

CR *cr_pi(void);
CR *cr_e(void);

CR *cr_add(CR *a, CR *b);
CR *cr_sub(CR *a, CR *b);
CR *cr_mul(CR *a, CR *b);
CR *cr_div(CR *a, CR *b);

CR *cr_neg(CR *a);
CR *cr_sqrt(CR *a);

CR *cr_pow(CR *b, CR *x);
CR *cr_exp(CR *x);
CR *cr_log(CR *b, CR *x);
CR *cr_ln(CR *x);

CR *cr_retain(CR *from);
void cr_release(CR *n);

void cr_eval(CR *n, mp_prec_t target_prec, mpfi_t result);
double cr_to_d(CR *n);

#endif
