#ifndef DIGIT_H
#define DIGIT_H

#include <stddef.h>
#include <gmp.h>

typedef enum
{
    DIGIT_OK,
    DIGIT_INVALID,
    DIGIT_OUT_OF_BASE,
} DigitError;

typedef enum
{
    DIGIT_FMT_ALNUM,
    DIGIT_FMT_LIST,
} DigitFormat;

typedef struct
{
    unsigned long *data;
    size_t len;
} Digits;

typedef struct
{
    const Digits *I;
    const Digits *N;
    const Digits *R;
} Literal;

void digits_init(Digits *ds, size_t len);
DigitError digits_from_alnum(Digits *ds, const char *s, size_t len, unsigned long base);
void digits_free(Digits *ds);

void literal_to_mpq(Literal *lit, unsigned long base, mpq_t out);

#endif
