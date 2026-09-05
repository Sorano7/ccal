#include "cut.h"
#include "parser.h"

#define PARSE(e, src, b) do { \
    if (e) expr_destroy(&e); \
    (e) = parse(SV(src), (b)); \
    if (is_error(e)) \
        CUT_FATAL("failed to parse "#src": ", \
                SV_ARG((e)->as.err)); \
} while (0)

#define EXPR_CHECK(got, want) do { \
    Expr *__e = (want); \
    if (!expr_equal((got), __e)) { \
        String __s; str_init(&__s); \
        expr_render(got, &__s); \
        CUT_ERROR("expression not equal:\n"SV_FMT, SV_ARG(SV(__s))); \
        str_free(&__s); \
    } \
    expr_destroy(&__e); \
} while (0)

#define PARSE_FAIL(src, b) do { \
    Expr *e = parse(SV(src), (b)); \
    if (!is_error(e)) \
        CUT_FATAL("did not failed on parsing "#src); \
    expr_destroy(&e); \
} while (0)

#define NUM_EQ(e, n, d) do { \
    CUT_MUST((e)->kind == EXPR_NUMBER); \
    CUT_CHECK(mpq_cmp_ui((e)->as.number, (n), (d)) == 0); \
} while (0)

TEST(parse_alnum_integer)
{
    Expr *e = NULL;

    PARSE(e, "123", 10);
    NUM_EQ(e, 123, 1);

    PARSE(e, "1A3", 16);
    NUM_EQ(e, 419, 1);

    PARSE(e, "ff", 16);
    NUM_EQ(e, 255, 1);

    expr_destroy(&e);
}

TEST(alnum_integer_digit_must_be_in_bounds)
{
    PARSE_FAIL("1234", 4);
    PARSE_FAIL("fG", 16);
    PARSE_FAIL("z", 60);
}

TEST(parse_digit_list_integer)
{
    Expr *e = NULL;

    PARSE(e, "[1,2,3]", 10);
    NUM_EQ(e, 123, 1);

    PARSE(e, "[1, 10, 3]", 16);
    NUM_EQ(e, 419, 1);

    expr_destroy(&e);
}

TEST(parse_decimal)
{
    Expr *e = NULL;

    PARSE(e, "0.25", 10);
    NUM_EQ(e, 1, 4);

    PARSE(e, "[0].([3])", 10);
    NUM_EQ(e, 1, 3);

    PARSE(e, "1.1(6)", 10);
    NUM_EQ(e, 7, 6);

    expr_destroy(&e);
}

TEST(underscore_ignored_in_literals)
{
    Expr *e = NULL;

    PARSE(e, "1_000_000", 10);
    NUM_EQ(e, 1000000, 1);

    expr_destroy(&e);
}

TEST(parse_identifier)
{
    Expr *e = NULL;

    PARSE(e, "\\foo", 10);
    CUT_CHECK(e->kind == EXPR_IDENT);
    CUT_CHECK(sv_equal(e->as.id, "\\foo"));

    expr_destroy(&e);
}

TEST(invalid_token_is_rejected)
{
    PARSE_FAIL("`", 10);
    PARSE_FAIL("|", 10);
    PARSE_FAIL("\"", 10);
}

TEST(infix_must_be_complete)
{
    PARSE_FAIL("12 + ", 10);
    PARSE_FAIL("* 34", 10);
}

TEST(digit_list_syntax_must_be_complete)
{
    PARSE_FAIL("[", 10);
    PARSE_FAIL("[]", 10);
    PARSE_FAIL("[,", 10);
    PARSE_FAIL("[1, 2", 10);
    PARSE_FAIL("[1, 2,]", 10);
}

TEST(decimal_syntax_must_be_complete)
{
    PARSE_FAIL("12.", 10);
    // Omitting 0 is not supported.
    PARSE_FAIL(".123", 10);
    PARSE_FAIL(".", 10);
    PARSE_FAIL("1.()", 10);
}

TEST(decimal_mixed_format_is_invalid)
{
    PARSE_FAIL("12.[3,4]", 10);
    PARSE_FAIL("[1,2].34([5,6])", 10);
}

TEST(group_must_be_closed)
{

    PARSE_FAIL("1 + (", 10);
    PARSE_FAIL("1 + (2", 10);
}

TEST(default_base_is_used_for_untagged_literal)
{
    Expr *e = NULL;

    PARSE(e, "10", 10);
    NUM_EQ(e, 10, 1);

    PARSE(e, "10", 8);
    NUM_EQ(e, 8, 1);

    PARSE(e, "10", 62);
    NUM_EQ(e, 62, 1);

    expr_destroy(&e);
}

TEST(leading_zero_base_prefix)
{
    Expr *e = NULL;

    PARSE(e, "0x10", 10);
    NUM_EQ(e, 16, 1);

    PARSE(e, "0b10", 10);
    NUM_EQ(e, 2, 1);

    PARSE(e, "0o10", 10);
    NUM_EQ(e, 8, 1);

    PARSE(e, "0d10", 255);
    NUM_EQ(e, 10, 1);

    expr_destroy(&e);
}

TEST(base_tag_binds_to_one_expression)
{
    Expr *e = NULL;

    PARSE(e, "16#10", 10);
    NUM_EQ(e, 16, 1);

    PARSE(e, "16#10 + 10", 10);
    EXPR_CHECK(e, 
        expr_infix(
            expr_number_ui((Span){0}, 16, 1),
            OP_ADD,
            expr_number_ui((Span){0}, 10, 1)
        ));

    PARSE(e, "10 + 16#10", 10);
    EXPR_CHECK(e, 
        expr_infix(
            expr_number_ui((Span){0}, 10, 1),
            OP_ADD,
            expr_number_ui((Span){0}, 16, 1)
        ));

    PARSE(e, "16#(10 + 10)", 10);
    EXPR_CHECK(e, 
        expr_infix(
            expr_number_ui((Span){0}, 16, 1),
            OP_ADD,
            expr_number_ui((Span){0}, 16, 1)
        ));

    PARSE_FAIL("16#-10", 10);

    expr_destroy(&e);
}

TEST(parse_infix_arithmetics)
{
    Expr *e = NULL;

    PARSE(e, "12 + 34", 10);
    EXPR_CHECK(e, 
        expr_infix(
            expr_number_ui((Span){0}, 12, 1),
            OP_ADD,
            expr_number_ui((Span){0}, 34, 1)
        ));

    PARSE(e, "100 - 75", 10);
    EXPR_CHECK(e, 
        expr_infix(
            expr_number_ui((Span){0}, 100, 1),
            OP_SUB,
            expr_number_ui((Span){0}, 75, 1)
        ));

    PARSE(e, "2 * 30", 10);
    EXPR_CHECK(e, 
        expr_infix(
            expr_number_ui((Span){0}, 2, 1),
            OP_MUL,
            expr_number_ui((Span){0}, 30, 1)
        ));

    PARSE(e, "5 / 7", 10);
    EXPR_CHECK(e, 
        expr_infix(
            expr_number_ui((Span){0}, 5, 1),
            OP_DIV,
            expr_number_ui((Span){0}, 7, 1)
        ));

    expr_destroy(&e);
}

TEST(parse_infix_comparison_and_equality)
{
    Expr *e = NULL;

    PARSE(e, "1 < 2", 10);
    CUT_MUST(e->kind == EXPR_INFIX);
    CUT_CHECK(e->as.infix.op == OP_LT);

    PARSE(e, "1 <= 2", 10);
    CUT_MUST(e->kind == EXPR_INFIX);
    CUT_CHECK(e->as.infix.op == OP_LEQ);

    PARSE(e, "1 > 2", 10);
    CUT_MUST(e->kind == EXPR_INFIX);
    CUT_CHECK(e->as.infix.op == OP_GT);

    PARSE(e, "1 >= 2", 10);
    CUT_MUST(e->kind == EXPR_INFIX);
    CUT_CHECK(e->as.infix.op == OP_GEQ);

    PARSE(e, "1 == 2", 10);
    CUT_MUST(e->kind == EXPR_INFIX);
    CUT_CHECK(e->as.infix.op == OP_EQ);

    PARSE(e, "1 != 2", 10);
    CUT_MUST(e->kind == EXPR_INFIX);
    CUT_CHECK(e->as.infix.op == OP_NEQ);

    expr_destroy(&e);
}

TEST(infix_has_correct_binding_power)
{
    Expr *e = NULL;

    PARSE(e, "20 * 3 + 1", 10);
    EXPR_CHECK(e, 
        expr_infix(
            expr_infix(
                expr_number_ui((Span){0}, 20, 1),
                OP_MUL,
                expr_number_ui((Span){0}, 3, 1)
            ),
            OP_ADD,
            expr_number_ui((Span){0}, 1, 1)
        ));

    PARSE(e, "2 + 3 * 4", 10);
    EXPR_CHECK(e, 
        expr_infix(
            expr_number_ui((Span){0}, 2, 1),
            OP_ADD,
            expr_infix(
                expr_number_ui((Span){0}, 3, 1),
                OP_MUL,
                expr_number_ui((Span){0}, 4, 1)
            )
        ));

    PARSE(e, "(2 + 3) * 4", 10);
    EXPR_CHECK(e, 
        expr_infix(
            expr_infix(
                expr_number_ui((Span){0}, 2, 1),
                OP_ADD,
                expr_number_ui((Span){0}, 3, 1)
            ),
            OP_MUL,
            expr_number_ui((Span){0}, 4, 1)
        ));

    expr_destroy(&e);
}

TEST(power_is_right_associative)
{
    Expr *e = NULL;

    PARSE(e, "2 ^ 3 ^ 2", 10);
    EXPR_CHECK(e, 
        expr_infix(
            expr_number_ui((Span){0}, 2, 1),
            OP_POW,
            expr_infix(
                expr_number_ui((Span){0}, 3, 1),
                OP_POW,
                expr_number_ui((Span){0}, 2, 1)
            )
        ));

    expr_destroy(&e);
}

TEST(assign_is_right_associative_and_left_must_be_id)
{
    Expr *e = NULL;

    PARSE(e, "\\x = \\y = 20", 10);

    EXPR_CHECK(e, 
        expr_infix(
            expr_id((Span){0}, SV("\\x")),
            OP_ASSIGN,
            expr_infix(
                expr_id((Span){0}, SV("\\y")),
                OP_ASSIGN,
                expr_number_ui((Span){0}, 20, 1)
            )
        ));

    expr_destroy(&e);
}

TEST(parse_lambda_expression)
{
    Expr *e = NULL;

    PARSE(e, "\\x : \\x + 1", 10);
    EXPR_CHECK(e,
        expr_lambda(
            expr_id((Span){0}, SV("\\x")),
            expr_infix(
                expr_id((Span){0}, SV("\\x")),
                OP_ADD,
                expr_number_ui((Span){0}, 1, 1)
            )
        ));

    PARSE(e, "\\x : \\y : \\x + \\y", 10);
    EXPR_CHECK(e,
        expr_lambda(
            expr_id((Span){0}, SV("\\x")),
            expr_lambda(
                expr_id((Span){0}, SV("\\y")),
                expr_infix(
                    expr_id((Span){0}, SV("\\x")),
                    OP_ADD,
                    expr_id((Span){0}, SV("\\y"))
                )
            )
        ));

    expr_destroy(&e);
}

TEST(application_precedence)
{
    Expr *e = NULL;

    // Note: valid AST since value is determined at run time.
    PARSE(e, "1 2 3", 10);
    EXPR_CHECK(e,
            expr_infix(
                expr_infix(
                    expr_number_ui((Span){0}, 1, 1),
                    OP_APPLY,
                    expr_number_ui((Span){0}, 2, 1)
                ),
                OP_APPLY,
                expr_number_ui((Span){0}, 3, 1)
            ));

    PARSE(e, "1 -2", 10);
    EXPR_CHECK(e,
            expr_infix(
                expr_number_ui((Span){0}, 1, 1),
                OP_APPLY,
                expr_prefix((Span){0},
                    OP_NEG,
                    expr_number_ui((Span){0}, 2, 1)
                )
            ));

    expr_destroy(&e);
}

TEST(parse_conditional)
{
    Expr *e = NULL;

    PARSE(e, "\\true ? 1 | 2", 10);
    EXPR_CHECK(e,
            expr_cond(
                expr_id((Span){0}, SV("\\true")),
                expr_number_ui((Span){0}, 1, 1),
                expr_number_ui((Span){0}, 2, 1)
            ));

    expr_destroy(&e);
}
