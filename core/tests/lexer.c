#include "cut.h"
#include "lexer.h"

#define START() \
    TokenList tokens; da_init(&tokens) \

#define END() \
    token_list_free(&tokens)

#define TOKENIZE(src) do { \
    token_list_reset(&tokens); \
    if (!tokenize(&tokens, SV(src), 0)) { \
        Token err = da_last(&tokens); \
        CUT_ERROR("failed to tokenize "#src": "SV_FMT, SV_ARG(SV(err.value))); \
    } \
} while (0)

#define FAIL(src) do { \
    token_list_reset(&tokens); \
    if (tokenize(&tokens, SV(src), 0)) \
        CUT_ERROR("did not failed to tokenize "#src); \
} while (0)

static void tkind_equal(TokenList *tl, TokenKind first, ...)
{
    va_list args;
    va_start(args, first);

    TokenKind current = first;
    DA_FOR(tl, i)
    {
        Token t = da_at(tl, i);

        if (current == TOK_EOF)
        {
            if (i < tl->len-1)
                DEV_ERROR("expected EOF at %zu, got %s", i, tk_to_str[t.kind]);
            break;
        }

        if (current != t.kind)
        {
            DEV_ERROR("expected %s at %zu, got %s",
                    tk_to_str[current], i, tk_to_str[t.kind]);
            return;
        }
        current = (TokenKind)va_arg(args, int);
    }

    va_end(args);
}

TEST(invalid_token)
{
    START();
        FAIL("@");
        FAIL("&");
    END();
}

TEST(tokenize_symbols)
{
    START();
        TOKENIZE("[ ] ( )");
        tkind_equal(&tokens, TOK_LBRAC, TOK_RBRAC, TOK_LPAREN, TOK_RPAREN, TOK_EOF);

        TOKENIZE(": , + $");
        tkind_equal(&tokens, TOK_COLON, TOK_COMMA, TOK_PLUS, TOK_DOLLAR, TOK_EOF);

        TOKENIZE("\n");
        tkind_equal(&tokens, TOK_NEWLINE, TOK_EOF);
    END();
}
