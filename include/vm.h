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

// Kinds of a value.
typedef enum
{
    VAL_VOID,
    VAL_ERROR,
    VAL_NUMBER,
    VAL_BOOL,
} ValueKind;

// A value that an expression can evaluate to.
typedef struct
{
    ValueKind kind;
    union
    {
        struct
        {
            size_t pos;
            char *msg;
        } error;

        mpq_t number;

        bool boolean;
    } as;
} Value;

void vm_init(VM *v);
void vm_free(VM *v);

bool vm_evaluate(VM *v, StringView src, Value *out);
void vm_value_print(Value *v, StringView src);
void vm_value_free(Value *v);

#endif
