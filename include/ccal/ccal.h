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
    bool          show_color;
    bool          show_rational;
} CCalCtx;

#define CCAL_CTX_DEFAULT (CCalCtx){10, 10, 10, 50, CCAL_FMT_AUTO, false, false}

CCalVM *ccal_create(void);
void ccal_free(CCalVM *vm);
void ccal_reset(CCalVM *vm);

#define CCAL_SETTER(T, id) void ccal_set_##id(CCalVM *vm, T id)
#define CCAL_GETTER(T, id) T ccal_get_##id(CCalVM *vm)

CCAL_SETTER(const CCalCtx *, ctx);
CCAL_SETTER(unsigned long,   ibase);
CCAL_SETTER(unsigned long,   obase);
CCAL_SETTER(unsigned long,   max_digits);
CCAL_SETTER(mp_prec_t,       prec);
CCAL_SETTER(CCalRenderFmt,   format);
CCAL_SETTER(bool,            show_color);
CCAL_SETTER(bool,            show_rational);

CCAL_GETTER(unsigned long,   ibase);
CCAL_GETTER(unsigned long,   obase);
CCAL_GETTER(unsigned long,   max_digits);
CCAL_GETTER(mp_prec_t,       prec);
CCAL_GETTER(CCalRenderFmt,   format);
CCAL_GETTER(bool,            show_color);
CCAL_GETTER(bool,            show_rational);

bool ccal_has_symbol(const CCalVM *vm, const char *id);
CCalValue *ccal_get_symbol(const CCalVM *vm, const char *id);

char **ccal_symbols(const CCalVM *vm, size_t *len);
void ccal_free_symbols(char **symbols, size_t len);

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
 * Host -> VM
 ************************************/

void ccal_set_global(CCalVM *vm, const char *id, CCalValue *val);


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
char *ccal_render_env(CCalVM *vm);

#endif
