#ifndef CCAL_H
#define CCAL_H

#include <stdbool.h>
#include <gmp.h>
#include <mpfr.h>


/************************************
 * Handles
 ************************************/

typedef struct CCalVM     CCalVM;
typedef struct CCalValue  CCalValue;


/************************************
 * VM/Interpreter
 ************************************/

typedef enum
{
    CCAL_FMT_AUTO,
    CCAL_FMT_RATIONAL,
    CCAL_FMT_FIXED_POINT,
    CCAL_FMT_SCIENTIFIC,
} CCalRenderFmt;

typedef struct
{
    unsigned long ibase;
    unsigned long obase;
    unsigned long max_digits;
    mp_prec_t     precision;
    CCalRenderFmt render_fmt;
} CCalCtx;

CCalVM *ccal_create(void);
void ccal_free(CCalVM *vm);
void ccal_reset(CCalVM *vm);

void ccal_set_ctx(CCalVM *vm, const CCalCtx *ctx);
void ccal_set_ibase(CCalVM *vm, unsigned long ibase);
void ccal_set_obase(CCalVM *vm, unsigned long obase);
void ccal_set_max_digits(CCalVM *vm, unsigned long max_digits);
void ccal_set_precision(CCalVM *vm, mp_prec_t prec);
void ccal_set_render_fmt(CCalVM *vm, CCalRenderFmt fmt);


/************************************
 * Value Handling
 ************************************/

typedef enum
{
    CCAL_VAL_VOID,
    CCAL_VAL_ERROR,
    CCAL_VAL_EXACT,
    CCAL_VAL_REAL,
    CCAL_VAL_LAMBDA,
} CCalValueKind;

CCalValueKind ccal_get_kind(const CCalValue *val);

CCalValue *ccal_retain(CCalValue *val);
void ccal_release(CCalValue *val);

CCalValue *ccal_exact_ui(unsigned long n, unsigned long d);
CCalValue *ccal_exact_q(const mpq_t q);
CCalValue *ccal_real_q(const mpq_t q);
CCalValue *ccal_bool(bool b);

bool ccal_equal(const CCalValue *a, const CCalValue *b);


/************************************
 * Parsing/Evaluating
 ************************************/

typedef enum
{
    CCAL_ERR_RUNTIME,
    CCAL_ERR_INCOMPLETE,
} CcalError;

typedef struct
{
    bool ok;
    CCalValue *value;
    CcalError error;
} CCalResult;

bool ccal_expr_complete(CCalVM *vm, const char *src);
CCalResult ccal_eval(CCalVM *vm, const char *src);


/************************************
 * String Rendering
 ************************************/

char *ccal_render(CCalVM *vm, const CCalValue *val);

#endif
