#include "digit.h"
#include <stdlib.h>
#include <assert.h>

// Convert a char to its digit number.
// Return ULONG_MAX if invalid.
static unsigned long char_to_num(char c)
{
    if (c >= '0' && c <= '9')
        return c - 48;

    if (c >= 'A' && c <= 'Z')
        return c - 55;

    else if (c >= 'a' && c <= 'z')
        return c - 61;

    else
        return ULONG_MAX;
}

// Initialize a literal without allocating the digit sequences.
void literal_init(Literal *lit)
{
    if (!lit) return;
    da_init(&lit->I);
    da_init(&lit->N);
    da_init(&lit->R);
}

// Free a literal.
void literal_free(Literal *lit)
{
    if (!lit) return;
    da_free(&lit->I);
    da_free(&lit->N);
    da_free(&lit->R);
}

static DigitResult digit_error(size_t pos, DRKind kind)
{
    return (DigitResult){pos=pos, .kind=kind};
}

// Create a sequnece of digits from an alphanumeric string.
DigitResult digits_from_alnum(DigitArray *ds, StringView s, unsigned long base)
{
    for (size_t i = 0; i < s.len; i++)
    {
        unsigned long val = char_to_num(s.data[i]);
        if (val == ULONG_MAX)
            return digit_error(i, DIGIT_INVALID);
        if (val >= base)
            return digit_error(i, DIGIT_OOB);

        da_append(ds, val);
    }
    return (DigitResult){.kind=DIGIT_OK};
}

// Converts a sequence of digits in the given base to an integer.
static void digits_to_mpz(const DigitArray *ds, unsigned long base, mpz_t out)
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
    assert(lit && lit->I.len > 0);

    mpz_t i_val, n_val, r_val;
    mpz_t b_n, b_r_1;
    mpz_t num, den, tmp;

    mpz_inits(i_val, n_val, r_val, b_n, b_r_1, num, den, tmp, NULL);

    digits_to_mpz(&lit->I, base, i_val);
    digits_to_mpz(&lit->N, base, n_val);
    digits_to_mpz(&lit->R, base, r_val);

    mpz_ui_pow_ui(b_n, base, lit->N.len);

    if (lit->R.len == 0)
    {
        mpz_mul(num, i_val, b_n);
        mpz_add(num, num, n_val);
        mpz_set(den, b_n);
    }
    else
    {
        mpz_ui_pow_ui(b_r_1, base, lit->R.len);
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
