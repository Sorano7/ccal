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

#define CHECK_FAIL(eval, show) do { \
    res = (eval); \
    CUT_CHECK(!res.ok); \
    if (show) CUT_DEBUG("%s", res.msg); \
} while (0)


/************************************
 * Number Literal
 ************************************/

TEST(alnum_integer_eval)
{
    FIXTURE_START();
        ENSURE_OK(vm_evaluate(&vm, "123", val));
        CUT_CHECK(mpq_cmp_ui(val, 123, 1) == 0);

        vm.base = 16;
        ENSURE_OK(vm_evaluate(&vm, "1A3", val));
        CUT_CHECK(mpq_cmp_ui(val, 419, 1) == 0);

        ENSURE_OK(vm_evaluate(&vm, "FF", val));
        CUT_CHECK(mpq_cmp_ui(val, 255, 1) == 0);

    FIXTURE_END();
}

TEST(alnum_integer_digit_must_be_in_bounds)
{
    FIXTURE_START();
        vm.base = 4;
        CHECK_FAIL(vm_evaluate(&vm, "1234", val), false);

        vm.base = 16;
        CHECK_FAIL(vm_evaluate(&vm, "FG", val), false);
    FIXTURE_END();
}

TEST(digit_list_integer_eval)
{
    FIXTURE_START();
        ENSURE_OK(vm_evaluate(&vm, "[1,2,3]", val));
        CUT_CHECK(mpq_cmp_ui(val, 123, 1) == 0);

        vm.base = 16;
        ENSURE_OK(vm_evaluate(&vm, "[1, 10, 3]", val));
        CUT_CHECK(mpq_cmp_ui(val, 419, 1) == 0);

    FIXTURE_END();
}

TEST(digit_list_syntax_must_be_complete)
{
    FIXTURE_START();
        CHECK_FAIL(vm_evaluate(&vm, "[", val), false);
        CHECK_FAIL(vm_evaluate(&vm, "[]", val), false);
        CHECK_FAIL(vm_evaluate(&vm, "[,", val), false);
        CHECK_FAIL(vm_evaluate(&vm, "[1, 2", val), false);
        CHECK_FAIL(vm_evaluate(&vm, "[1, 2,]", val), false);
    FIXTURE_END();
}

TEST(decimal_eval)
{
    FIXTURE_START();
        ENSURE_OK(vm_evaluate(&vm, "0.25", val));
        CUT_CHECK(mpq_cmp_ui(val, 1, 4) == 0);

        ENSURE_OK(vm_evaluate(&vm, "[0].([3])", val));
        CUT_CHECK(mpq_cmp_ui(val, 1, 3) == 0);

        ENSURE_OK(vm_evaluate(&vm, "1.1(6)", val));
        CUT_CHECK(mpq_cmp_ui(val, 7, 6) == 0);
    FIXTURE_END();
}

TEST(decimal_syntax_must_be_complete)
{
    FIXTURE_START();
        CHECK_FAIL(vm_evaluate(&vm, "12.", val), false);
        // Omitting 0 is not supported.
        CHECK_FAIL(vm_evaluate(&vm, ".123", val), false);
        CHECK_FAIL(vm_evaluate(&vm, ".", val), false);
        CHECK_FAIL(vm_evaluate(&vm, "1.()", val), false);
    FIXTURE_END();
}

TEST(decimal_mixed_format_is_invalid)
{
    FIXTURE_START();
        CHECK_FAIL(vm_evaluate(&vm, "12.[3,4]", val), false);
        CHECK_FAIL(vm_evaluate(&vm, "[1,2].34([5,6])", val), false);
    FIXTURE_END();
}


/************************************
 * Base Annotation
 ************************************/

TEST(default_base_is_used_for_untagged_literal)
{
    FIXTURE_START();
        ENSURE_OK(vm_evaluate(&vm, "10", val));
        CUT_CHECK(mpq_cmp_ui(val, 10, 1) == 0);

        vm.base = 8;
        ENSURE_OK(vm_evaluate(&vm, "10", val));
        CUT_CHECK(mpq_cmp_ui(val, 8, 1) == 0);

        vm.base = 100;
        ENSURE_OK(vm_evaluate(&vm, "10", val));
        CUT_CHECK(mpq_cmp_ui(val, 100, 1) == 0);
    FIXTURE_END();
}

TEST(base_tag_binds_to_one_expression)
{
    FIXTURE_START();
        ENSURE_OK(vm_evaluate(&vm, "16#10", val));
        CUT_CHECK(mpq_cmp_ui(val, 16, 1) == 0);

        ENSURE_OK(vm_evaluate(&vm, "16#10 + 10", val));
        CUT_CHECK(mpq_cmp_ui(val, 26, 1) == 0);

        ENSURE_OK(vm_evaluate(&vm, "10 + 16#10", val));
        CUT_CHECK(mpq_cmp_ui(val, 26, 1) == 0);

        ENSURE_OK(vm_evaluate(&vm, "16#(10 + 10)", val));
        CUT_CHECK(mpq_cmp_ui(val, 32, 1) == 0);
    FIXTURE_END();
}

TEST(base_tag_must_be_closest_to_expression)
{
    FIXTURE_START();
        CHECK_FAIL(vm_evaluate(&vm, "16#-10", val), false);
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

        CHECK_FAIL(vm_evaluate(&vm, "1 / 0", val), false);
    FIXTURE_END();
}

TEST(infix_has_correct_binding_power)
{
    FIXTURE_START();
        ENSURE_OK(vm_evaluate(&vm, "20 * 3 + 1", val));
        CUT_CHECK(mpq_cmp_ui(val, 61, 1) == 0);

        ENSURE_OK(vm_evaluate(&vm, "2 + 3 * 4", val));
        CUT_CHECK(mpq_cmp_ui(val, 14, 1) == 0);

        ENSURE_OK(vm_evaluate(&vm, "(2 + 3) * 4", val));
        CUT_CHECK(mpq_cmp_ui(val, 20, 1) == 0);
    FIXTURE_END();
}

TEST_RUN()
