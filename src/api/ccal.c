#include "ccal/ccal.h"
#include "vm.h"

/************************************
 * Internal wrappers
 ************************************/

typedef struct CCalVM
{
    VM vm;
    Source src;
} CCalVM;

typedef struct CCalValue
{
    Value *value;
} CCalValue;


/************************************
 * VM/Interpreter
 ************************************/

CCalVM *ccal_create(void)
{
    CCalVM *vm = malloc(sizeof(CCalVM));
    vm_init(&vm->vm);
    source_init(&vm->src);
    vm->vm.ctx.src = &vm->src;
    return vm;
}

void ccal_free(CCalVM *vm)
{
    if (!vm) return;
    vm_free(&vm->vm);
    source_free(&vm->src);
}

void ccal_reset(CCalVM *vm)
{
    vm_reset(&vm->vm);
    source_reset(&vm->src);
}

CCAL_SETTER(const CCalCtx *, ctx)
{
    ccal_set_ibase(vm, ctx->ibase);
    ccal_set_obase(vm, ctx->obase);
    ccal_set_max_digits(vm, ctx->max_digits);
    ccal_set_prec(vm, ctx->precision);
    ccal_set_format(vm, ctx->render_fmt);
    ccal_set_show_color(vm, ctx->show_color);
    ccal_set_show_rational(vm, ctx->show_rational);
}

CCAL_SETTER(unsigned long, ibase)      { if (ibase > 1) vm->vm.ctx.ibase = ibase; }
CCAL_SETTER(unsigned long, obase)      { if (obase > 1) vm->vm.ctx.obase = obase; }
CCAL_SETTER(unsigned long, max_digits) { vm->vm.ctx.max_digits = max_digits; }
CCAL_SETTER(mp_prec_t, prec)           { vm->vm.ctx.prec = prec; }
CCAL_SETTER(bool, show_color)          { vm->vm.ctx.use_color = show_color; }
CCAL_SETTER(bool, show_rational)       { vm->vm.ctx.show_rational = show_rational; }

CCAL_SETTER(CCalRenderFmt, format)
{
    switch (format)
    {
        case CCAL_FMT_AUTO:        vm->vm.ctx.fmt = FMT_AUTO;  break;
        case CCAL_FMT_FIXED_POINT: vm->vm.ctx.fmt = FMT_FIXED; break;
        case CCAL_FMT_SCIENTIFIC:  vm->vm.ctx.fmt = FMT_SCI;   break;
    }
}

CCAL_GETTER(unsigned long, ibase)      { return vm->vm.ctx.ibase; }
CCAL_GETTER(unsigned long, obase)      { return vm->vm.ctx.obase; }
CCAL_GETTER(unsigned long, max_digits) { return vm->vm.ctx.max_digits; }
CCAL_GETTER(mp_prec_t, prec)           { return vm->vm.ctx.prec; }
CCAL_GETTER(bool, show_color)        { return vm->vm.ctx.use_color; }
CCAL_GETTER(bool, show_rational)       { return vm->vm.ctx.show_rational; }

CCAL_GETTER(CCalRenderFmt, format)
{
    switch (vm->vm.ctx.fmt)
    {
        case FMT_AUTO:  return CCAL_FMT_AUTO;
        case FMT_FIXED: return CCAL_FMT_FIXED_POINT;
        case FMT_SCI:   return CCAL_FMT_SCIENTIFIC;
    }
}


/************************************
 * Value Handling
 ************************************/

CCalValueKind ccal_get_kind(const CCalValue *val)
{
    switch (val->value->kind)
    {
        case VAL_VOID:    return CCAL_VAL_VOID;
        case VAL_EXACT:   return CCAL_VAL_EXACT;
        case VAL_REAL:    return CCAL_VAL_REAL;
        case VAL_ERROR:   return CCAL_VAL_ERROR;
        case VAL_NATIVE:
        case VAL_LAMBDA:  return CCAL_VAL_LAMBDA;
        default:          UNREACHABLE();
    }
}

static CCalValue *value_wrap(Value *val)
{
    CCalValue *out = malloc(sizeof(CCalValue));
    out->value = val;
    return out;
}

CCalValue *ccal_retain(CCalValue *val)
{
    if (!val) return val;
    val->value = value_retain(val->value);
    return val;
}

void ccal_release(CCalValue *val)
{
    if (!val) return;
    value_release(&val->value);
}

CCalValue *ccal_exact_ui(unsigned long n, unsigned long d)
{
    mpq_t q;
    mpq_init(q);
    mpq_set_ui(q, n, d);

    CCalValue *out = ccal_exact_q(q);
    mpq_clear(q);
    return out;
}

CCalValue *ccal_exact_q(const mpq_t q)
{
    return value_wrap(value_exact((Span){0}, q));
}

CCalValue *ccal_real_q(const mpq_t q)
{
    CR *n = cr_from_mpq(q);
    return value_wrap(value_real((Span){0}, n));
}

CCalValue *ccal_bool(bool b)
{
    return value_wrap(value_bool((Span){0}, b));
}

bool ccal_equal(const CCalValue *a, const CCalValue *b)
{
    return value_equal(a->value, b->value);
}


/************************************
 * Host -> VM
 ************************************/

void ccal_set_global(CCalVM *vm, const char *id, CCalValue *val)
{
    if (!val || !val->value) return;
    scope_set_symbol(vm->vm.scope, SV(id), val->value);
}


/************************************
 * Parsing/Evaluating
 ************************************/

bool ccal_expr_complete(CCalVM *vm, const char *src)
{
    return vm_is_complete(&vm->vm, SV(src));
}

static CCalResult result_error(CCalValue *err, CcalError code)
{
    return (CCalResult){.ok=false, .value=err, .error=code};
}

static CCalResult result_ok(CCalValue *val)
{
    return (CCalResult){.ok=true, .value=val};
}

CCalResult ccal_eval(CCalVM *vm, const char *src)
{
    Source *s = ccal_expr_complete(vm, src) ? &vm->src : NULL;
    Value *val = vm_run(&vm->vm, SV(src), s);
    CCalValue *out = malloc(sizeof(CCalValue));
    out->value = val;

    if (value_is_err(val))
        return result_error(out, CCAL_ERR_RUNTIME);

    return result_ok(out);
}


/************************************
 * String Rendering
 ************************************/

char *ccal_render(CCalVM *vm, const CCalValue *val)
{
    String sb;
    str_init(&sb);

    vm_value_render(&vm->vm, val->value, &sb);

    return sb.data;
}

char *ccal_render_env(CCalVM *vm)
{
    String sb;
    str_init(&sb);

    vm_env_render(&vm->vm, &sb);

    return sb.data;
}
