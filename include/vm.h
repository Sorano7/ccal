#ifndef VM_H
#define VM_H

#include "lexer.h"
#include <gmp.h>

// A VM for just-in-time evaluation.
typedef struct
{
    unsigned long base;
    TokenArray *ta;
    size_t pos;
} VM;

typedef struct
{
    bool ok;
    size_t span_start;
    char *msg;
} VMResult;

void vm_init(VM *v);
void vm_free(VM *v);

VMResult vm_evaluate(VM *v, const char *src, mpq_t out);
void vm_diagnostics(VMResult *r, const char *src);
void vm_result_free(VMResult *r);

#endif
