#include "number.h"
#include <stdlib.h>
#include <assert.h>

#define DIGIT_TO_0     48
#define UPPER_TO_10    55
#define LOWER_TO_36    61
#define LOWER_TO_10    87
#define BASE_MAX_ALNUM 62

// Convert a char to its digit number.
// Return ULONG_MAX if invalid.
static unsigned long char_to_num(char c, unsigned long base)
{
    if (c >= '0' && c <= '9')
        return c - DIGIT_TO_0;

    if (c >= 'A' && c <= 'Z')
        return c - UPPER_TO_10;

    if (c >= 'a' && c <= 'z')
    {
        if (base > 36)
            return c - LOWER_TO_36;
        return c - LOWER_TO_10;
    }

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
    if (base > BASE_MAX_ALNUM)
        return (DigitResult){.kind=DIGIT_BASE_TOO_LARGE, .pos=0};
    for (size_t i = 0; i < s.len; i++)
    {
        if (s.data[i] == '_') continue;
        unsigned long val = char_to_num(s.data[i], base);
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

typedef struct
{
    mpz_t rem;
    size_t pos;
} Rem;

typedef struct
{
    Rem *data;
    size_t cap;
    size_t len;
} RemList;

// Render a rational as a decimal string truncated up to max_digits.
// Assumes base up to 62.
void render_decimal(String *sb, const mpq_t n, int base, size_t max_digits)
{
    if (mpq_sgn(n) < 0)
        str_append(sb, "-");

    mpz_t num, den, intpart, rem, mul, digit;
    mpz_inits(num, den, intpart, rem, mul, digit, NULL);

    mpz_abs(num, mpq_numref(n));
    mpz_set(den, mpq_denref(n));


    mpz_fdiv_qr(intpart, rem, num, den);
    {
        char *s = mpz_get_str(NULL, base, intpart);
        str_append(sb, s);
        free(s);
    }

    if (mpz_sgn(rem) != 0)
    {
        str_append(sb, ".");

        String frac;
        str_init(&frac);

        RemList seen;
        da_init(&seen);

        size_t pos = 0;
        bool truncated = false;
        size_t repeat_start = SIZE_MAX;

        // Long division
        for (;;)
        {
            if (mpz_sgn(rem) == 0) break;
            if (max_digits > 0 && pos >= max_digits)
            {
                truncated = true;
                break;
            }

            // Search whether remainder is seen
            size_t found = SIZE_MAX;
            DA_FOR(&seen, i)
            {
                Rem r = da_at(&seen, i);
                if (mpz_cmp(r.rem, rem) == 0)
                {
                    found = r.pos;
                    break;
                }
            }
            if (found != SIZE_MAX)
            {
                repeat_start = found;
                break;
            }

            // New remainder
            Rem r;
            mpz_init(r.rem);
            mpz_set(r.rem, rem);
            r.pos = pos;
            da_append(&seen, r);

            mpz_mul_ui(mul, rem, base);
            mpz_fdiv_qr(digit, rem, mul, den);
            {
                char *s = mpz_get_str(NULL, base, digit);
                str_append(&frac, s);
                free(s);
            }

            pos++;
        }

        DA_FOR(&seen, i) mpz_clear(da_at(&seen, i).rem);
        da_free(&seen);

        if (repeat_start != SIZE_MAX)
        {
            str_appendf(sb, "%.*s(%s)", 
                    (int)repeat_start, frac.data, 
                    frac.data+repeat_start);
        }
        else
        {
            str_append(sb, &frac);
            if (truncated) str_append(sb, "...");
        }
    }

    mpz_clears(num, den, intpart, rem, mul, digit, NULL);
}
