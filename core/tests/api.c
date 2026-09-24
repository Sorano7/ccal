#include "cut.h"
#include "../include/ccal/ccal.h"

#define EVAL(vm, src) do { \
    CCalResult res = ccal_eval((vm), (src)); \
    if (!res.ok) \
        CUT_FATAL("failed to evaluate "#src); \
} while (0)

#define EVAL_FAIL(vm, src, code) do { \
    CCalResult res = ccal_eval((vm), (src)); \
    if (res.ok) \
        CUT_FATAL("did not fail to evaluate "#src); \
    if (res.error != (code)) \
        CUT_ERROR("expected error code %d, got %d", (code), res.error); \
} while (0)

#define EVAL_EQ(vm, src, want) do { \
    CCalValue *__want = (want); \
    CCalResult res = ccal_eval((vm), (src)); \
    if (!res.ok) \
        CUT_FATAL("failed to evaluate "#src); \
    if (!ccal_equal(res.value, __want)) { \
        char *got_str = ccal_render(vm, res.value); \
        char *want_str = ccal_render(vm, __want); \
        CUT_ERROR("expected %s, got %s", want_str, got_str); \
        free(got_str); free(want_str); \
    } \
    ccal_release(__want); \
} while (0)


TEST(api_basic_eval_test)
{
    CCalVM *vm = ccal_create();

    EVAL_EQ(vm, "1 + 1", ccal_exact_ui(2, 1));
    EVAL_EQ(vm, "'true", ccal_bool(true));

    ccal_set_ibase(vm, 16);
    EVAL_EQ(vm, "a + b", ccal_exact_ui(21, 1));

    ccal_free(vm);
}

TEST(host_set_global_variable)
{
    CCalVM *vm = ccal_create();

    EVAL_FAIL(vm, "'foo", CCAL_ERR_RUNTIME);

    CCalValue *val = ccal_exact_ui(100, 1);
    ccal_set_global(vm, "foo", val);

    EVAL_EQ(vm, "'foo", val);

    ccal_release(val);
    ccal_free(vm);
}

CCAL_NATIVE_FN(native_eq)
{
    (void)vm, (void)ud;

    CCalValue *l = argv[0];
    CCalValue *r = argv[1];
    bool v = ccal_equal(l, r);
    return ccal_bool(v);
}

TEST(host_native_fn)
{
    CCalVM *vm =ccal_create();

    EVAL_FAIL(vm, "1 `my_eq` 2", CCAL_ERR_RUNTIME);

    CCalNative *eq = ccal_native(vm, native_eq, 2, NULL);
    ccal_set_native(vm, "my_eq", eq);

    EVAL_EQ(vm, "1 `my_eq` 2", ccal_bool(false));
    EVAL_EQ(vm, "'true `my_eq` 'true", ccal_bool(true));

    ccal_native_free(eq);
    ccal_free(vm);
}
