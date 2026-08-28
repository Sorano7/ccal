#ifndef NUMBER_H
#define NUMBER_H

#include <stddef.h>
#include <gmp.h>
#include "cut.h"

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

#endif
