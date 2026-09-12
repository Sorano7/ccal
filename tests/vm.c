#include "vm.h"
#include <gmp.h>

#include "cut.h"

#define START() \
    String sb; str_init(&sb); \
    RenderCtx ctx = { \
        .prec = 50, \
        .max_digits=10, \
        .base=10, \
        .num_form=NUMBER_RATIONAL, \
        .src=SV(""), \
        .use_color=false, \
    }; \
    (void)ctx; \
    VM vm; vm_init(&vm); \
    Value *val = NULL;

#define END() \
    str_free(&sb); \
    value_release(val);

#define EVAL(src) do { \
    if (val) value_release(val); \
    val = vm_run(&vm, SV(src)); \
    if (value_is_err(val)) \
        CUT_FATAL("failed to parse "#src": "SV_FMT, \
                SV_ARG(SV(val->as.error))); \
} while (0)

#define EVAL_RENDER(s) do { \
    if (val) value_release(val); \
    str_reset(&sb); \
    EVAL(s); \
    ctx.src = SV(s); \
    value_render(val, &sb, &ctx); \
} while (0)

#define EVAL_FAIL(src) do { \
    val = vm_run(&vm, SV(src)); \
    if (!value_is_err(val)) \
        CUT_FATAL("did not failed on parsing "#src); \
    value_release(val); \
} while (0)

#define NUM_EQ(v, n, d) do { \
    CUT_MUST((v)->kind == VAL_EXACT); \
    if (mpq_cmp_ui((v)->as.exact, (n), (d)) != 0) { \
        char *s = mpq_get_str(NULL, 10, (v)->as.exact); \
        CUT_ERROR("expected %d/%d, found %s", n, d, s); \
        free(s); \
    } \
} while (0)

#define BOOL_EQ(v, b) do { \
    CUT_MUST(value_is_bool(v)); \
    CUT_CHECK(value_to_bool(v) == (b)); \
} while (0)


TEST(basic_arithmetics)
{
    START();
        EVAL("2 + 3 * 4");
        NUM_EQ(val, 14, 1);

        EVAL("(2 + 3) * 4");
        NUM_EQ(val, 20, 1);

        EVAL("2 ^ 10");
        NUM_EQ(val, 1024, 1);

        // (255 + 10*15) - 35 + (5/6 * 6) - (2/3 * 3)
        EVAL("16#(ff + 2#1010 * 8#17) - 36#z + 0.8(3) * 6 - 0.(6) * 3");
        NUM_EQ(val, 373, 1);
    END();
}

TEST(boolean_render_correct)
{
    START();
        EVAL_RENDER("999 * 0.5 < 666 * 0.8");
        CUT_CHECK(sv_equal(sb, "'true"));

        EVAL_RENDER("999 * 0.5 > 666 * 0.8");
        CUT_CHECK(sv_equal(sb, "'false"));
    END();
}

TEST(integer_render_correct)
{
    START();
        EVAL_RENDER("123");
        CUT_CHECK(sv_equal(sb, "123"));

        EVAL_RENDER("16#FF");
        CUT_CHECK(sv_equal(sb, "255"));

        ctx.base = 16;
        EVAL_RENDER("255");
        // GMP defaults to lowercase for base <= 36
        CUT_CHECK(sv_equal(sb, "16#ff"));
    END();
}

TEST(rational_render_correct)
{
    START();
        EVAL_RENDER("0.3");
        CUT_CHECK(sv_equal(sb, "3/10"));

        EVAL_RENDER("20 / 30");
        CUT_CHECK(sv_equal(sb, "2/3"));
    END();
}

TEST(decimal_render_correct)
{
    START();
        ctx.num_form = NUMBER_DECIMAL;
        EVAL_RENDER("0.3");
        CUT_CHECK(sv_equal(sb, "0.3"));

        EVAL_RENDER("1/3");
        CUT_CHECK(sv_equal(sb, "0.(3)"));

        EVAL_RENDER("5/6");
        CUT_CHECK(sv_equal(sb, "0.8(3)"));
    END();
}

TEST(variable_assign_and_evaluate)
{
    START();
        EVAL("'x = 1");
        NUM_EQ(val, 1, 1);

        EVAL("'x");
        NUM_EQ(val, 1, 1);

        EVAL("'x = 200");
        NUM_EQ(val, 200, 1);

        EVAL("'x");
        NUM_EQ(val, 200, 1);
    END();
}

TEST(variable_dynamic_typing)
{
    START();
        EVAL("'x = 1");
        NUM_EQ(val, 1, 1);

        EVAL("'x = 'x > 1");
        BOOL_EQ(val, false);
    END();
}

TEST(cannot_access_unknown_variable)
{
    START();
        EVAL_FAIL("'x");
    END();
}

TEST(builtin_constants_eval)
{
    START();
        EVAL("'true");
        BOOL_EQ(val, true);

        EVAL("'false");
        BOOL_EQ(val, false);
    END();
}

TEST(cannot_assign_to_builtin)
{
    START();
        EVAL_FAIL("'true = 1");
        EVAL_FAIL("'ans = 'true");
    END();
}

TEST(lambda_call_eval)
{
    START();
        EVAL("('x: 'x * 2) 2");
        NUM_EQ(val, 4, 1);

        EVAL("('x: 'y: 'x + 'y) 2 4");
        NUM_EQ(val, 6, 1);

        EVAL("('x: 'x * 2) $ ('x: 'x + 2) 2");
        NUM_EQ(val, 8, 1);

        EVAL_FAIL("('x: 'x 2) 2");
    END();
}

TEST(conditional_eval)
{
    START();
        EVAL("'true ? 1 | 2");
        NUM_EQ(val, 1, 1);

        EVAL("'x = 1");
        EVAL("(2 > 1) ? 1 | ('x = 2)");
        NUM_EQ(val, 1, 1);
        EVAL("'x");
        NUM_EQ(val, 1, 1);
    END();
}

TEST(recursion_eval)
{
    START();
        EVAL("'fact = 'n: 'n == 0 ? 1 | 'n * 'fact ('n - 1)");

        EVAL("'fact 0");
        NUM_EQ(val, 1, 1);

        EVAL("'fact 2");
        NUM_EQ(val, 2, 1);

        EVAL("'fact 5");
        NUM_EQ(val, 120, 1);
    END();
}

TEST(boolean_application)
{
    START();
        EVAL("'true");
        BOOL_EQ(val, true);

        EVAL("'true 1");
        CUT_CHECK(val->kind == VAL_BUILTIN);
        CUT_CHECK(val->as.builtin.args.len == 1);

        EVAL("'true 1 2");
        NUM_EQ(val, 1, 1);
    END();
}

TEST(hole_identifier)
{
    START();
        EVAL("'_");
        CUT_CHECK(val->kind == VAL_VOID);

        EVAL("'_ = 100");
        NUM_EQ(val, 100, 1);

        EVAL("'_");
        CUT_CHECK(val->kind == VAL_VOID);
    END();
}

TEST(multi_expression)
{
    START();
        EVAL("'x = 100; 'x * 2");
        NUM_EQ(val, 200, 1);
    END();
}

TEST(infix_application)
{
    START();
        EVAL("'div = 'x: 'y: 'x / 'y");
        EVAL("1 `div` 2");
        NUM_EQ(val, 1, 2);
    END();
}

TEST(sqrt_eval)
{
    START();
        EVAL_RENDER("'sqrt 2");
        CUT_CHECK(sv_equal(sb, "1.414213562..."));

        EVAL_RENDER("'sqrt 7");
        CUT_CHECK(sv_equal(sb, "2.645751311..."));

        EVAL_RENDER("'sqrt 9");
        CUT_CHECK(sv_equal(sb, "3"));
    END();
}

TEST(exact_and_real_operation)
{
    START();
        EVAL_RENDER("'sqrt 2 + 2");
        CUT_CHECK(sv_equal(sb, "3.414213562..."));
    END();
}
