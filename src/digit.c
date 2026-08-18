#include "digit.h"
#include <stdlib.h>
#include <assert.h>

// Initialize a digit sequence.
void digits_init(Digits *ds, size_t len)
{
    assert(ds);
    ds->data = malloc(len);
    ds->len = len;
}

// Convert a char to its digit number.
// Return ULONG_MAX if invalid.
static unsigned long char_to_num(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - 48;
    }
    if (c >= 'A' && c <= 'Z')
    {
        return c - 55;
    }
    else if (c >= 'a' && c <= 'z')
    {
        return c - 61;
    }
    else
    {
        return ULONG_MAX;
    }
}

// Create a sequnece of digits from an alphanumeric string.
DigitError digits_from_alnum(Digits *ds, const char *s, size_t len, unsigned long base)
{
    digits_init(ds, len);

    for (size_t i = 0; i < len; i++)
    {
        unsigned long val = char_to_num(s[i]);
        if (val == ULONG_MAX) return DIGIT_INVALID;
        if (val >= base) return DIGIT_OUT_OF_BASE;

        ds->data[i] = val;
    }
    return DIGIT_OK;
}

void digits_free(Digits *ds)
{
    if (!ds) return;
    if (ds->data) free(ds->data);
    ds->len = 0;
}

// Converts a sequence of digits in the given base to an integer.
static void digits_to_mpz(const Digits *ds, unsigned long base, mpz_t out)
{
    mpz_set_ui(out, 0);
    if (!ds) return;

    for (size_t i = 0; i < ds->len; i++)
    {
        mpz_mul_ui(out, out, base);
        mpz_add_ui(out, out, ds->data[i]);
    }
}

// Convert a literal to a rational.
void literal_to_mpq(Literal *lit, unsigned long base, mpq_t out)
{
    assert(lit && lit->I);

    mpz_t i_val, n_val, r_val;
    mpz_t b_n, b_r_1;
    mpz_t num, den, tmp;

    mpz_inits(i_val, n_val, r_val, b_n, b_r_1, num, den, tmp, NULL);

    size_t n = lit->N ? lit->N->len: 0;
    size_t r = lit->R ? lit->R->len: 0;

    digits_to_mpz(lit->I, base, i_val);
    digits_to_mpz(lit->N, base, n_val);
    digits_to_mpz(lit->R, base, r_val);

    mpz_ui_pow_ui(b_n, base, n);

    if (r == 0)
    {
        mpz_mul(num, i_val, b_n);
        mpz_add(num, num, n_val);
        mpz_set(den, b_n);
    }
    else
    {
        mpz_ui_pow_ui(b_r_1, base, r);
        mpz_sub_ui(b_r_1, b_r_1, 1);

        mpz_mul(den, b_n, b_r_1);

        mpz_mul(num, i_val, den);
        mpz_mul(tmp, n_val, b_r_1);
        mpz_add(num, num, tmp);
        mpz_add(num, num, r_val);
    }

    mpq_set_num(out, num);
    mpq_set_den(out, den);
    mpq_canonicalize(out);

    mpz_clears(i_val, n_val, r_val, b_n, b_r_1, num, den, tmp, NULL);
}
