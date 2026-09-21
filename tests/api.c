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
