#include "vm.h"
#include <gmp.h>
#include <math.h>

#include "cut.h"

#define START() \
    String sb; str_init(&sb); \
    RenderCtx ctx = { \
        .prec          = 50, \
        .max_digits    = 10, \
        .base          = 10, \
        .fmt           = FMT_AUTO, \
        .show_rational = false, \
        .use_color     = false, \
        .src           = NULL, \
    }; \
    (void)ctx; \
    VM vm; vm_init(&vm); \
    Value *val = NULL;

#define END() \
    str_free(&sb); \
    value_release(&val);

#define EVAL(src) do { \
    if (val) value_release(&val); \
    val = vm_run(&vm, SV(src), NULL); \
    if (value_is_err(val)) \
        CUT_FATAL("failed to run "#src": "SV_FMT, \
                SV_ARG(SV(val->as.error))); \
} while (0)

#define EVAL_RENDER(s) do { \
    str_reset(&sb); \
    EVAL(s); \
    value_render(val, &sb, &ctx); \
} while (0)

#define EVAL_FAIL(src) do { \
    val = vm_run(&vm, SV(src), NULL); \
    if (!value_is_err(val)) \
        CUT_FATAL("did not failed on running "#src); \
    value_release(&val); \
} while (0)

#define NUM_EQ(v, n, d) do { \
    CUT_MUST((v)->kind == VAL_EXACT); \
    if (mpq_cmp_ui((v)->as.exact, (n), (d)) != 0) { \
        char *s = mpq_get_str(NULL, 10, (v)->as.exact); \
        CUT_ERROR("expected %d/%d, found %s", n, d, s); \
        free(s); \
    } \
} while (0)

#define REAL_CLOSE(v, d) do { \
    CUT_MUST((v)->kind == VAL_REAL); \
    double got = cr_to_d((v)->as.real); \
    if (fabs(got - (d)) > 1e-12) \
        CUT_ERROR("expected %.10g, found %.10g", (d), got); \
} while (0)

#define BOOL_EQ(v, b) do { \
    CUT_MUST(value_is_bool(v)); \
    CUT_CHECK(value_to_bool(v) == (b)); \
} while (0)

#define RENDER_EQ(got, want) do { \
    if (!sv_equal(got, want)) \
        CUT_ERROR("expected \""SV_FMT"\", found \""SV_FMT"\"", \
                SV_ARG(SV(want)), SV_ARG(SV(got))); \
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
        RENDER_EQ(sb, "'true");

        EVAL_RENDER("999 * 0.5 > 666 * 0.8");
        RENDER_EQ(sb, "'false");
    END();
}

TEST(integer_render_correct)
{
    START();
        EVAL_RENDER("123");
        RENDER_EQ(sb, "123");

        EVAL_RENDER("16#FF");
        RENDER_EQ(sb, "255");

        ctx.base = 16;
        EVAL_RENDER("255");
        // GMP defaults to lowercase for base <= 36
        RENDER_EQ(sb, "16#ff");
    END();
}

TEST(rational_render_correct)
{
    START();
        ctx.show_rational = true;
        EVAL_RENDER("0.3");
        RENDER_EQ(sb, "0.3 or 3/10");

        EVAL_RENDER("20 / 30");
        RENDER_EQ(sb, "0.(6) or 2/3");
    END();
}

TEST(decimal_render_correct)
{
    START();
        EVAL_RENDER("0.3");
        RENDER_EQ(sb, "0.3");

        EVAL_RENDER("1/3");
        RENDER_EQ(sb, "0.(3)");

        EVAL_RENDER("5/6");
        RENDER_EQ(sb, "0.8(3)");
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

TEST(lambda_capture_by_value)
{
    START();
        EVAL("'foo = 100; ('x: 'foo + 'x) 1");
        NUM_EQ(val, 101, 1);

        EVAL("'foo = 100; 'bar = 'x: 'foo + 'x; 'foo = 200");
        EVAL("'bar 1");
        NUM_EQ(val, 101, 1);
    END();
}

TEST(lambda_rejects_undefined_symbol)
{
    START();
        EVAL_FAIL("('x: 'y)");
        EVAL("('x: ('y = 'x))");
    END();
}

TEST(conditional_eval)
{
    START();
        EVAL("'true ? 1 : 2");
        NUM_EQ(val, 1, 1);

        EVAL("'x = 1");
        EVAL("(2 > 1) ? 1 : ('x = 2)");
        NUM_EQ(val, 1, 1);
        EVAL("'x");
        NUM_EQ(val, 1, 1);
    END();
}

TEST(recursion_eval)
{
    START();
        EVAL("'f = 'n: 'n == 0 ? 1 : 'n * 'f ('n - 1)");
        EVAL("'f 0");
        NUM_EQ(val, 1, 1);
        EVAL("'f 2");
        NUM_EQ(val, 2, 1);
        EVAL("'f 5");
        NUM_EQ(val, 120, 1);

        // alias
        EVAL("'g = 'f; 'g 5");
        NUM_EQ(val, 120, 1);

        // inner recursion
        EVAL("'foo = 'x: ('g = 'n: 'n == 0 ? 1 : 'n * 'g ('n - 1)) 'x");
        EVAL("'foo 5");
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

TEST(exact_converts_to_real_when_involved)
{
    START();
        EVAL("'sqrt 4 * 2");
        REAL_CLOSE(val, 4.0);
    END();
}

TEST(real_approximation)
{
    START();
        EVAL("'pi ~= 'pi");
        BOOL_EQ(val, true);

        EVAL("'ln 'e ~= 2");
        BOOL_EQ(val, false);
    END();
}

TEST(real_render_correct)
{
    START();
        EVAL_RENDER("'pow 2 3");
        RENDER_EQ(sb, "~= 8.0000000000");
    END();
}

TEST(power_rejects_large_exponent)
{
    START();
        EVAL_FAIL("2 ^ (2^64-1)");
        EVAL_FAIL("2 `pow` 2 `pow` 32");
    END();
}

TEST(power_converts_to_real)
{
    START();
        EVAL("2 ^ 2");
        NUM_EQ(val, 4, 1);

        EVAL("2 ^ 0.5 ~= 2 `pow` 0.5");
        BOOL_EQ(val, true);
    END();
}

TEST(sqrt_rejects_negative)
{
    START();
        EVAL_FAIL("'sqrt -1");
    END();
}

TEST(ln_log_rejects_negative_and_zero)
{
    START();
        EVAL_FAIL("'ln 0");
        EVAL_FAIL("'log 2 -2");
    END();
}
