#ifndef VM_H
#define VM_H

#include "value.h"

typedef struct
{
    unsigned long base;
    Value *last;
    Scope *scope;
} VM;

void vm_env_render(VM *v, String *sb, RenderCtx *ctx);

void vm_init(VM *v);
void vm_reset(VM *v);
void vm_free(VM *v);

Value *vm_run_next(VM *v, StringView line, size_t offset);
Value *vm_run(VM *v, StringView input, Source *src);

#endif
