#include "vm.h"
#include <gmp.h>

#include "cut.h"

#define START() \
    String sb; str_init(&sb); \
    RenderCtx ctx = { \
        .max_digits=50, \
        .base=10, \
        .num_form=NUMBER_RATIONAL, \
        .src=SV(""), \
        .use_color=false, \
    }; \
    (void)ctx; \
    VM vm; vm_init(&vm); \
    Value val = {0};

#define END() \
    str_free(&sb); \
    vm_value_free(&val);

#define EVAL(src, v) do { \
    if (!vm_run(&vm, SV(src), (v))) \
        CUT_FATAL(SV_FMT, SV_ARG(SV(val.as.error))); \
} while (0)

#define EVAL_RENDER(s, v) do { \
    str_reset(&sb); \
    EVAL((s), (v)); \
    ctx.src = SV(s); \
    vm_value_render(&val, &sb, &ctx); \
} while (0)

#define EVAL_FAIL(src) do { \
    bool ok = vm_run(&vm, SV(src), &val); \
    if (ok) CUT_ERROR(#src " did not fail"); \
} while (0)

#define NUM_EQ(v, n, d) do { \
    CUT_MUST((v)->kind == VAL_NUMBER); \
    if (mpq_cmp_ui((v)->as.number, (n), (d)) != 0) { \
        char *s = mpq_get_str(NULL, 10, (v)->as.number); \
        CUT_ERROR("expected %d/%d, found %s", s); \
        free(s); \
    } \
} while (0)

#define BOOL_EQ(v, b) do { \
    CUT_MUST((v)->kind == VAL_BOOL); \
    CUT_CHECK((v)->as.boolean == (b)); \
} while (0)


TEST(basic_arithmetics)
{
    START();
        EVAL("2 + 3 * 4", &val);
        NUM_EQ(&val, 14, 1);

        EVAL("(2 + 3) * 4", &val);
        NUM_EQ(&val, 20, 1);

        EVAL("2 ^ 10", &val);
        NUM_EQ(&val, 1024, 1);

        // (255 + 10*15) - 35 + (5/6 * 6) - (2/3 * 3)
        EVAL("16#(ff + 2#1010 * 8#17) - 36#z + 0.8(3) * 6 - 0.(6) * 3", &val);
        NUM_EQ(&val, 373, 1);
    END();
}

TEST(boolean_render_correct)
{
    START();
        EVAL_RENDER("999 * 0.5 < 666 * 0.9", &val);
        CUT_CHECK(sv_equal(sb, "@true"));

        EVAL_RENDER("999 * 0.5 > 666 * 0.9", &val);
        CUT_CHECK(sv_equal(sb, "@false"));
    END();
}

TEST(integer_render_correct)
{
    START();
        EVAL_RENDER("123", &val);
        CUT_CHECK(sv_equal(sb, "123"));

        EVAL_RENDER("16#FF", &val);
        CUT_CHECK(sv_equal(sb, "255"));

        ctx.base = 16;
        EVAL_RENDER("255", &val);
        // GMP defaults to lowercase for base <= 36
        CUT_CHECK(sv_equal(sb, "16#ff"));
    END();
}

TEST(rational_render_correct)
{
    START();
        EVAL_RENDER("0.3", &val);
        CUT_CHECK(sv_equal(sb, "3/10"));

        EVAL_RENDER("20 / 30", &val);
        CUT_CHECK(sv_equal(sb, "2/3"));
    END();
}

TEST(decimal_render_correct)
{
    START();
        ctx.num_form = NUMBER_DECIMAL;
        EVAL_RENDER("0.3", &val);
        CUT_CHECK(sv_equal(sb, "0.3"));

        EVAL_RENDER("1/3", &val);
        CUT_CHECK(sv_equal(sb, "0.(3)"));

        EVAL_RENDER("5/6", &val);
        CUT_CHECK(sv_equal(sb, "0.8(3)"));
    END();
}

TEST(variable_assign_and_evaluate)
{
    START();
        EVAL("@x = 1", &val);
        NUM_EQ(&val, 1, 1);

        EVAL("@x", &val);
        NUM_EQ(&val, 1, 1);

        EVAL("@x = 200", &val);
        NUM_EQ(&val, 200, 1);

        EVAL("@x", &val);
        NUM_EQ(&val, 200, 1);
    END();
}

TEST(variable_dynamic_typing)
{
    START();
        EVAL("@x = 1", &val);
        NUM_EQ(&val, 1, 1);

        EVAL("@x = @x > 1", &val);
        BOOL_EQ(&val, false);
    END();
}

TEST(cannot_access_unknown_variable)
{
    START();
        EVAL_FAIL("@x");
    END();
}

TEST(builtin_constants_eval)
{
    START();
        EVAL("@true", &val);
        BOOL_EQ(&val, true);

        EVAL("@false", &val);
        BOOL_EQ(&val, false);
    END();
}

TEST(cannot_assign_to_builtin)
{
    START();
        EVAL_FAIL("@true = 1");
        EVAL_FAIL("@@ = @true");
    END();
}
