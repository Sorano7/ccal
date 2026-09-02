#include "vm.h"
#include <gmp.h>

#include "cut.h"

#define FIXTURE_START() \
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

#define FIXTURE_END() \
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


/************************************
 * Value Rendering
 ************************************/

TEST(boolean_render_correct)
{
    FIXTURE_START();
        EVAL_RENDER("999 * 0.5 < 666 * 0.9", &val);
        CUT_CHECK(sv_equal(sb, "@true"));

        EVAL_RENDER("999 * 0.5 > 666 * 0.9", &val);
        CUT_CHECK(sv_equal(sb, "@false"));
    FIXTURE_END();
}

TEST(integer_render_correct)
{
    FIXTURE_START();
        EVAL_RENDER("123", &val);
        CUT_CHECK(sv_equal(sb, "123"));

        EVAL_RENDER("16#FF", &val);
        CUT_CHECK(sv_equal(sb, "255"));

        ctx.base = 16;
        EVAL_RENDER("255", &val);
        // GMP defaults to lowercase for base <= 36
        CUT_CHECK(sv_equal(sb, "16#ff"));
    FIXTURE_END();
}

TEST(rational_render_correct)
{
    FIXTURE_START();
        EVAL_RENDER("0.3", &val);
        CUT_CHECK(sv_equal(sb, "3/10"));

        EVAL_RENDER("20 / 30", &val);
        CUT_CHECK(sv_equal(sb, "2/3"));
    FIXTURE_END();
}

TEST(decimal_render_correct)
{
    FIXTURE_START();
        ctx.num_form = NUMBER_DECIMAL;
        EVAL_RENDER("0.3", &val);
        CUT_CHECK(sv_equal(sb, "0.3"));

        EVAL_RENDER("1/3", &val);
        CUT_CHECK(sv_equal(sb, "0.(3)"));

        EVAL_RENDER("5/6", &val);
        CUT_CHECK(sv_equal(sb, "0.8(3)"));
    FIXTURE_END();
}

TEST(variable_assign_and_evaluate)
{
    FIXTURE_START();
        EVAL("@x = 1", &val);
        NUM_EQ(&val, 1, 1);

        EVAL("@x", &val);
        NUM_EQ(&val, 1, 1);

        EVAL("@x = 200", &val);
        NUM_EQ(&val, 200, 1);

        EVAL("@x", &val);
        NUM_EQ(&val, 200, 1);
    FIXTURE_END();
}

TEST(variable_dynamic_typing)
{
    FIXTURE_START();
        EVAL("@x = 1", &val);
        NUM_EQ(&val, 1, 1);

        EVAL("@x = @x > 1", &val);
        BOOL_EQ(&val, false);
    FIXTURE_END();
}

TEST(cannot_access_unknown_variable)
{
    FIXTURE_START();
        EVAL_FAIL("@x");
    FIXTURE_END();
}

TEST(builtin_constants_eval)
{
    FIXTURE_START();
        EVAL("@true", &val);
        BOOL_EQ(&val, true);

        EVAL("@false", &val);
        BOOL_EQ(&val, false);
    FIXTURE_END();
}

TEST(cannot_assign_to_builtin)
{
    FIXTURE_START();
        EVAL_FAIL("@true = 1");
        EVAL_FAIL("@@ = @true");
    FIXTURE_END();
}
