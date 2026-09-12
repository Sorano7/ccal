#include "cut.h"
#include "creal.h"
#include <math.h>

#define CLOSE(cr, want) do { \
    double d = cr_to_d(cr); \
    if (fabs(d - (want)) > 1e-12) \
        CUT_ERROR("got: %.15g, want: %.15g", d, want); \
} while (0)

TEST(creal_basic_arithmetics)
{
    CR *c1 = cr_from_si(1, 1);
    CR *c2 = cr_from_si(2, 1);

    CR *sum = cr_add(c1, c2);
    CLOSE(sum, 3.0);
    CR *diff = cr_sub(c1, c2);
    CLOSE(diff, -1.0);
    CR *prod = cr_mul(c1, c2);
    CLOSE(prod, 2.0);
    CR *quot = cr_div(c1, c2);
    CLOSE(quot, 0.5);

    cr_release(c1);
    cr_release(c2);
    cr_release(sum);
    cr_release(diff);
    cr_release(prod);
    cr_release(quot);
}

TEST(creal_sqrt)
{
    CR *c2 = cr_from_si(2, 1);
    CR *root = cr_sqrt(c2);
    CLOSE(root, 1.4142135623730951);

    cr_release(c2);
    cr_release(root);
}

TEST(creal_ln_exp)
{
    CR *ce = cr_e();
    CR *c1 = cr_from_si(1, 1);

    CR *exp1 = cr_exp(c1);
    CLOSE(exp1, 2.718281828459045);

    CR *ln_e = cr_ln(ce);
    CLOSE(ln_e, 1.0);

    cr_release(ce);
    cr_release(c1);
    cr_release(exp1);
    cr_release(ln_e);
}

TEST(creal_pow_log)
{
    CR *c2 = cr_from_si(2, 1);
    CR *c3 = cr_from_si(3, 1);
    CR *c8 = cr_from_si(8, 1);

    CR *pow = cr_pow(c2, c3);
    CLOSE(pow, 8.0);

    CR *log = cr_log(c2, c8);
    CLOSE(log, 3.0);

    cr_release(c2);
    cr_release(c3);
    cr_release(c8);
    cr_release(pow);
    cr_release(log);
}
