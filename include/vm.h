#ifndef VM_H
#define VM_H

#include "lexer.h"
#include <gmp.h>

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
            String msg;
        } error;

        mpq_t number;

        bool boolean;
    } as;
} Value;

// A symbol to value binding.
typedef struct
{
    String *id;
    Value value;
} Symbol;

// An environment scope.
typedef struct
{
    Symbol *data;
    size_t len;
    size_t cap;
} Scope;

// An array of scope.
typedef struct
{
    Scope **data;
    size_t len;
    size_t cap;
} Env;

typedef enum
{
    NUMBER_DECIMAL,
    NUMBER_RATIONAL,
} NumberForm;

typedef struct
{
    StringView src;
    NumberForm num_form;
    unsigned long max_digits;
    unsigned long base;
    bool use_color;
} RenderCtx;

// A VM for just-in-time evaluation.
typedef struct
{
    unsigned long base;
    TokenArray *ta;
    size_t pos;
    Env *env;
} VM;

void vm_init(VM *v);
void vm_reset(VM *v);
void vm_free(VM *v);

bool vm_evaluate(VM *v, StringView src, Value *out);
void vm_value_render(Value *v, String *sb, RenderCtx *ctx);
void vm_value_free(Value *v);

void vm_env_render(VM *v, String *sb, RenderCtx *ctx);

#endif
