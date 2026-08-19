#ifndef DIGIT_H
#define DIGIT_H

#include <stddef.h>
#include <gmp.h>

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
    Digits I;
    Digits N;
    Digits R;
} Literal;

typedef enum
{
    DIGIT_OK,
    DIGIT_INVALID,
    DIGIT_OOB,
} DRKind;

typedef struct
{
    size_t pos;
    DRKind kind;
} DigitResult;

void digits_alloc(Digits *ds, size_t len);

DigitResult digits_from_alnum(Digits *ds, const char *s, size_t len, unsigned long base);

void literal_init(Literal *lit);
void literal_free(Literal *lit);
void literal_to_mpq(Literal *lit, unsigned long base, mpq_t out);

#endif
