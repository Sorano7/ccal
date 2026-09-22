#include "ccal/ccal.h"
#include "vm.h"

/************************************
 * Internal wrappers
 ************************************/

typedef struct CCalVM
{
    VM vm;
    Source src;
    RenderCtx ctx;
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
    render_ctx_default(&vm->ctx);
    vm->ctx.src = &vm->src;
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
    render_ctx_default(&vm->ctx);
}

void ccal_set_ctx(CCalVM *vm, const CCalCtx *ctx)
{
    ccal_set_ibase(vm, ctx->ibase);
    ccal_set_obase(vm, ctx->obase);
    ccal_set_max_digits(vm, ctx->max_digits);
    ccal_set_precision(vm, ctx->precision);
    ccal_set_render_fmt(vm, ctx->render_fmt);
}

void ccal_set_ibase(CCalVM *vm, unsigned long ibase)
{
    if (ibase > 1) vm->vm.base = ibase;
}

void ccal_set_obase(CCalVM *vm, unsigned long obase)
{
    if (obase > 1) vm->ctx.base = obase;
}

void ccal_set_max_digits(CCalVM *vm, unsigned long max_digits)
{
    vm->ctx.max_digits = max_digits;
}

void ccal_set_precision(CCalVM *vm, mp_prec_t prec)
{
    vm->ctx.prec = prec;
}

void ccal_set_render_fmt(CCalVM *vm, CCalRenderFmt fmt)
{
    switch (fmt)
    {
        case CCAL_FMT_RATIONAL:
        case CCAL_FMT_AUTO:        vm->ctx.fmt = FMT_AUTO;  break;
        case CCAL_FMT_FIXED_POINT: vm->ctx.fmt = FMT_FIXED; break;
        case CCAL_FMT_SCIENTIFIC:  vm->ctx.fmt = FMT_SCI;   break;
    }
    vm->ctx.show_rational = fmt == CCAL_FMT_RATIONAL;
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
        case VAL_BUILTIN:
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

    value_render(val->value, &sb, &vm->ctx);

    return sb.data;
}
