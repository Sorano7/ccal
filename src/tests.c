#define CUT_IMPLEMENTATION
#include "cut.h"

#include "vm.h"
#include <gmp.h>

#define FIXTURE_START() \
    VM vm; vm_init(&vm); \
    mpq_t val; mpq_init(val); \
    VMResult res; \

#define FIXTURE_END() \
    mpq_clear(val); \
    vm_result_free(&res) \

#define GMP_DEBUG(fmt, ...) do { \
    int len = gmp_snprintf(NULL, 0, (fmt), __VA_ARGS__); \
    char __tmp[len+1]; \
    gmp_snprintf(__tmp, len+1, (fmt), __VA_ARGS__); \
    CUT_DEBUG("%s", __tmp); \
} while (0)

#define ENSURE_OK(eval) do { \
    res = (eval); \
    if (!res.ok) CUT_FATAL("%s", res.msg); \
} while (0)


/************************************
 * Number Literal
 ************************************/

TEST(numeric_integer_eval)
{
    FIXTURE_START();
        ENSURE_OK(vm_evaluate(&vm, "123", val));
        CUT_CHECK(mpq_cmp_ui(val, 123, 1) == 0);
    FIXTURE_END();
}

TEST(alnum_integer_eval)
{
    FIXTURE_START();
        vm.base = 16;
        ENSURE_OK(vm_evaluate(&vm, "FF", val));
        CUT_CHECK(mpq_cmp_ui(val, 255, 1) == 0);
    FIXTURE_END();
}

TEST(digit_list_integer_eval)
{
    FIXTURE_START();
        ENSURE_OK(vm_evaluate(&vm, "[1,2,3]", val));
        CUT_CHECK(mpq_cmp_ui(val, 123, 1) == 0);
    FIXTURE_END();
}


/************************************
 * Basic Arithmetics
 ************************************/

TEST(integer_addition_eval)
{
    FIXTURE_START();
        ENSURE_OK(vm_evaluate(&vm, "12 + 34", val));
        CUT_CHECK(mpq_cmp_ui(val, 46, 1) == 0);
    FIXTURE_END();
}

TEST(integer_subtraction_eval)
{
    FIXTURE_START();
        ENSURE_OK(vm_evaluate(&vm, "100 - 75", val));
        CUT_CHECK(mpq_cmp_ui(val, 25, 1) == 0);
    FIXTURE_END();
}

TEST(integer_multiplication_eval)
{
    FIXTURE_START();
        ENSURE_OK(vm_evaluate(&vm, "2 * 10 * 30", val));
        CUT_CHECK(mpq_cmp_ui(val, 600, 1) == 0);
    FIXTURE_END();
}

TEST(integer_division_eval)
{
    FIXTURE_START();
        ENSURE_OK(vm_evaluate(&vm, "100 / 50", val));
        CUT_CHECK(mpq_cmp_ui(val, 2, 1) == 0);

        res = vm_evaluate(&vm, "1 / 0", val);
        CUT_MUST(!res.ok);
    FIXTURE_END();
}

TEST_RUN()
