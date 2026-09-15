#ifndef NUMBER_H
#define NUMBER_H

#include <stddef.h>
#include <mpfi.h>
#include <gmp.h>
#include "cut.h"

#define BASE_DEFAULT 10
#define BASE_MAX ULONG_MAX

typedef enum
{
    DIGIT_FMT_ALNUM,
    DIGIT_FMT_LIST,
} DigitFormat;

typedef struct
{
    unsigned long *data;
    size_t len;
    size_t cap;
} DigitArray;

typedef struct
{
    DigitArray I;
    DigitArray N;
    DigitArray R;
} Literal;

typedef enum
{
    DIGIT_OK,
    DIGIT_INVALID,
    DIGIT_OOB,
    DIGIT_BASE_TOO_LARGE,
} DRKind;

typedef struct
{
    size_t pos;
    DRKind kind;
} DigitResult;

DigitResult digits_from_alnum(DigitArray *ds, StringView s, unsigned long base);

void literal_init(Literal *lit);
void literal_free(Literal *lit);
void literal_to_mpq(Literal *lit, unsigned long base, mpq_t out);

typedef enum
{
    FMT_AUTO,
    FMT_FIXED,
    FMT_SCI,
} OutputFormat;

void render_mpq_as_decimal(String *sb, const mpq_t n, int base, size_t max_digits);
void render_mpfi(String *sb, const mpfi_t n, int base, size_t max_digits, OutputFormat fmt);

#define RENDER_BITS_MAX 1e6

double bit_estimate(double b, double x);
double bit_estimate_mpq(const mpq_t b, const mpq_t x);

#endif
