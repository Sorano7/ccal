#ifndef EVAL_H
#define EVAL_H

#include "expr.h"

// Result of evaluation.
typedef struct
{
    bool ok;
    const char *msg;
} EvalResult;

EvalResult evaluate(Expr *e, mpq_t out);

#endif
