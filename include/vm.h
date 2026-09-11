#ifndef VM_H
#define VM_H

#include "value.h"
#include <gmp.h>


// A VM for just-in-time evaluation.
typedef struct
{
    unsigned long base;
    Value *last;
    Scope *scope;
} VM;

void vm_init(VM *v);
void vm_reset(VM *v);
void vm_free(VM *v);

bool vm_run(VM *v, StringView src, Value *out);
bool vm_eval_expr(VM *v, Expr *e, Value *out);

void vm_env_render(VM *v, String *sb, RenderCtx *ctx);

#endif
