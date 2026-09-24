#ifndef VM_H
#define VM_H

#include "value.h"
#include "number.h"

typedef struct
{
    unsigned long ibase;
    unsigned long obase;

    Source *src;

    OutputFormat fmt;
    bool show_rational;

    mp_prec_t prec;
    unsigned long max_digits;

    bool use_color;
} VMCtx;

typedef struct
{
    String id;
    NativeFn fn;
    size_t arity;
    void *ud;
} NativeEntry;

DA_DEFINE(NativeTable, NativeEntry);

typedef struct VM
{
    Value *last;
    Scope *scope;
    NativeTable natives;

    VMCtx ctx;
} VM;

void vm_init(VM *v);
void vm_reset(VM *v);
void vm_free(VM *v);

void vm_set_native(VM *v, StringView id, NativeFn fn, size_t arity, void *ud);

bool vm_is_complete(VM *v, StringView src);
Value *vm_run_next(VM *v, StringView line, size_t offset);
Value *vm_run(VM *v, StringView input, Source *src);

void vm_value_render(VM *v, Value *val, String *sb);
void vm_env_render(VM *v, String *sb);

#endif
