#include "vm.h"
#include <gmp.h>

#define CUT_IMPL
#include "cut.h"

#define FIXTURE_START() \
    String sb; str_init(&sb); \
    RenderCtx ctx = {.num_form=NUMBER_RATIONAL,.src=SV("")}; \
    (void)ctx; \
    VM vm; vm_init(&vm); \
    Value val = {0};

#define FIXTURE_END() \
    str_free(&sb); \
    vm_value_free(&val);

#define GMP_DEBUG(fmt, ...) do { \
    int len = gmp_snprintf(NULL, 0, (fmt), __VA_ARGS__); \
    char __tmp[len+1]; \
    gmp_snprintf(__tmp, len+1, (fmt), __VA_ARGS__); \
    CUT_DEBUG("%s", __tmp); \
} while (0)

#define EVAL(src, v) do { \
    if (!vm_evaluate(&vm, SV(src), (v))) \
        CUT_FATAL("%s", val.as.error.msg); \
} while (0)

#define EVAL_RENDER(s, v) do { \
    str_reset(&sb); \
    EVAL((s), (v)); \
    ctx.src = SV(s); \
    vm_value_render(&val, &sb, &ctx); \
} while (0)

#define EVAL_FAIL(src, v) do { \
    bool ok = vm_evaluate(&vm, SV(src), (v)); \
    if (ok) CUT_ERROR(#src " did not fail"); \
} while (0)

#define NUMBER_EQ(v, n, d) do { \
    CUT_MUST((v)->kind == VAL_NUMBER); \
    CUT_CHECK(mpq_cmp_ui((v)->as.number, (n), (d)) == 0); \
} while (0)

#define BOOL_EQ(v, b) do { \
    CUT_MUST((v)->kind == VAL_BOOL); \
    CUT_CHECK((v)->as.boolean == (b)); \
} while (0)


/************************************
 * Number Literal
 ************************************/

TEST(alnum_integer_eval)
{
    FIXTURE_START();
        EVAL("123", &val);
        NUMBER_EQ(&val, 123, 1);

        vm.base = 16;
        EVAL("1A3", &val);
        NUMBER_EQ(&val, 419, 1);

        EVAL("FF", &val);
        NUMBER_EQ(&val, 255, 1);

    FIXTURE_END();
}

TEST(alnum_integer_digit_must_be_in_bounds)
{
    FIXTURE_START();
        vm.base = 4;
        EVAL_FAIL("1234", &val);

        vm.base = 16;
        EVAL_FAIL("FG", &val);
    FIXTURE_END();
}

TEST(digit_list_integer_eval)
{
    FIXTURE_START();
        EVAL("[1,2,3]", &val);
        NUMBER_EQ(&val, 123, 1);

        vm.base = 16;
        EVAL("[1, 10, 3]", &val);
        NUMBER_EQ(&val, 419, 1);

    FIXTURE_END();
}

TEST(decimal_eval)
{
    FIXTURE_START();
        EVAL("0.25", &val);
        NUMBER_EQ(&val, 1, 4);

        EVAL("[0].([3])", &val);
        NUMBER_EQ(&val, 1, 3);

        EVAL("1.1(6)", &val);
        NUMBER_EQ(&val, 7, 6);
    FIXTURE_END();
}


/************************************
 * Syntax Validation
 ************************************/

TEST(invalid_token_is_rejected)
{
    FIXTURE_START();
        EVAL_FAIL("&", &val);
        EVAL_FAIL("\\", &val);
    FIXTURE_END();
}

TEST(nud_must_not_precede_nud)
{
    FIXTURE_START();
        EVAL_FAIL("123 123", &val);
        EVAL_FAIL("123 [1,2,3]", &val);
        EVAL_FAIL("123 (-123)", &val);
    FIXTURE_END();
}

TEST(infix_must_be_complete)
{
    FIXTURE_START();
        EVAL_FAIL("12 + ", &val);
        EVAL_FAIL("* 12", &val);
    FIXTURE_END();
}

TEST(digit_list_syntax_must_be_complete)
{
    FIXTURE_START();
        EVAL_FAIL("[", &val);
        EVAL_FAIL("[]", &val);
        EVAL_FAIL("[,", &val);
        EVAL_FAIL("[1, 2", &val);
        EVAL_FAIL("[1, 2,]", &val);
    FIXTURE_END();
}

TEST(decimal_syntax_must_be_complete)
{
    FIXTURE_START();
        EVAL_FAIL("12.", &val);
        // Omitting 0 is not supported.
        EVAL_FAIL(".123", &val);
        EVAL_FAIL(".", &val);
        EVAL_FAIL("1.()", &val);
    FIXTURE_END();
}

TEST(decimal_mixed_format_is_invalid)
{
    FIXTURE_START();
        EVAL_FAIL("12.[3,4]", &val);
        EVAL_FAIL("[1,2].34([5,6])", &val);
    FIXTURE_END();
}

TEST(group_must_be_closed)
{
    FIXTURE_START();
        EVAL_FAIL("1 + (", &val);
        EVAL_FAIL("1 + (2", &val);
    FIXTURE_END();
}


/************************************
 * Base Annotation
 ************************************/

TEST(default_base_is_used_for_untagged_literal)
{
    FIXTURE_START();
        EVAL("10", &val);
        NUMBER_EQ(&val, 10, 1);

        vm.base = 8;
        EVAL("10", &val);
        NUMBER_EQ(&val, 8, 1);

        vm.base = 100;
        EVAL("10", &val);
        NUMBER_EQ(&val, 100, 1);
    FIXTURE_END();
}

TEST(base_tag_binds_to_one_expression)
{
    FIXTURE_START();
        EVAL("16#10", &val);
        NUMBER_EQ(&val, 16, 1);

        EVAL("16#10 + 10", &val);
        NUMBER_EQ(&val, 26, 1);

        EVAL("10 + 16#10", &val);
        NUMBER_EQ(&val, 26, 1);

        EVAL("16#(10 + 10)", &val);
        NUMBER_EQ(&val, 32, 1);
    FIXTURE_END();
}

TEST(base_tag_must_be_closest_to_expression)
{
    FIXTURE_START();
        EVAL_FAIL("16#-10", &val);
    FIXTURE_END();
}


/************************************
 * Basic Arithmetics
 ************************************/

TEST(integer_addition_eval)
{
    FIXTURE_START();
        EVAL("12 + 34", &val);
        NUMBER_EQ(&val, 46, 1);
    FIXTURE_END();
}

TEST(integer_subtraction_eval)
{
    FIXTURE_START();
        EVAL("100 - 75", &val);
        NUMBER_EQ(&val, 25, 1);
    FIXTURE_END();
}

TEST(integer_multiplication_eval)
{
    FIXTURE_START();
        EVAL("2 * 10 * 30", &val);
        NUMBER_EQ(&val, 600, 1);
    FIXTURE_END();
}

TEST(integer_division_eval)
{
    FIXTURE_START();
        EVAL("100 / 50", &val);
        NUMBER_EQ(&val, 2, 1);

        EVAL_FAIL("1 / 0", &val);
    FIXTURE_END();
}

TEST(infix_has_correct_binding_power)
{
    FIXTURE_START();
        EVAL("20 * 3 + 1", &val);
        NUMBER_EQ(&val, 61, 1);

        EVAL("2 + 3 * 4", &val);
        NUMBER_EQ(&val, 14, 1);

        EVAL("(2 + 3) * 4", &val);
        NUMBER_EQ(&val, 20, 1);
    FIXTURE_END();
}


/************************************
 * Boolean
 ************************************/

TEST(comparison_operators_eval)
{
    FIXTURE_START();
        EVAL("1 < 2", &val);
        BOOL_EQ(&val, true);

        EVAL("2 <= 2", &val);
        BOOL_EQ(&val, true);

        EVAL("1 > 2", &val);
        BOOL_EQ(&val, false);

        EVAL("2 >= 2", &val);
        BOOL_EQ(&val, true);
    FIXTURE_END();
}

TEST(equality_operators_eval)
{
    FIXTURE_START();
        EVAL("1 == 2", &val);
        BOOL_EQ(&val, false);

        EVAL("1 + 1 == 2", &val);
        BOOL_EQ(&val, true);

        EVAL("(1 == 1) == (2 == 2)", &val);
        BOOL_EQ(&val, true);
    FIXTURE_END();
}


/************************************
 * Value Rendering
 ************************************/

TEST(boolean_render_correct)
{
    FIXTURE_START();
        EVAL_RENDER("999 * 0.5 < 666 * 0.9", &val);
        sv_equal(sb, "true");

        EVAL_RENDER("999 * 0.5 > 666 * 0.9", &val);
        sv_equal(sb, "false");
    FIXTURE_END();
}

TEST(integer_render_correct)
{
    FIXTURE_START();
        EVAL_RENDER("123", &val);
        sv_equal(sb, "123");

        TODO("change output base");
        EVAL_RENDER("16#FF", &val);
        sv_equal(sb, "255");
    FIXTURE_END();
}

TEST(rational_render_correct)
{
    FIXTURE_START();
        EVAL_RENDER("0.3", &val);
        sv_equal(sb, "3/10");

        EVAL_RENDER("20 / 30", &val);
        sv_equal(sb, "2/3");
    FIXTURE_END();
}

TEST(decimal_render_correct)
{
    FIXTURE_START();
        ctx.num_form = NUMBER_DECIMAL;
        EVAL_RENDER("0.3", &val);
        sv_equal(sb, "0.3");

        EVAL_RENDER("1/3", &val);
        sv_equal(sb, "0.(3)");

        EVAL_RENDER("5/6", &val);
        sv_equal(sb, "0.8(3)");
    FIXTURE_END();
}


TEST_RUN()
