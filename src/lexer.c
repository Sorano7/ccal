#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

const char *tk_to_str[] = {TOKENS(AS_STR)};

#define NEXT_IS_THEN(src, i, c, tk) do { \
    if ((src).data[(i)+1] == (c)) return (tk); \
} while (0)

// Get the kind of token at the pointer.
static TokenKind token_kind_get(StringView src, size_t i)
{
    if (isdigit(src.data[i])) return TOK_DIGIT;
    if (isalpha(src.data[i])) return TOK_ALPHA;
    if (isspace(src.data[i])) return TOK_SPACE;

    switch (src.data[i])
    {
        case '+': return TOK_PLUS;
        case '-': return TOK_MINUS;
        case '*': return TOK_STAR;
        case '/': return TOK_SLASH;
        case '^': return TOK_CARET;

        case '.': return TOK_DOT;
        case ',': return TOK_COMMA;
        case '#': return TOK_HASH;

        case '[': return TOK_LBRAC;
        case ']': return TOK_RBRAC;
        case '(': return TOK_LPAREN;
        case ')': return TOK_RPAREN;

        case '=':
            NEXT_IS_THEN(src, i, '=', TOK_EQ);
            break;

        case '!':
            NEXT_IS_THEN(src, i, '=', TOK_NEQ);
            break;

        case '<':
            NEXT_IS_THEN(src, i, '=', TOK_LEQ);
            return TOK_LT;

        case '>':
            NEXT_IS_THEN(src, i, '=', TOK_GEQ);
            return TOK_GT;
    }

    return TOK_INVALID;
}

// Create a token.
static Token token_create(TokenKind kind, StringView value, size_t pos)
{
    return (Token){.value=value, .pos=pos, .kind=kind};
}

// Tokenize the source.
bool tokenize(TokenArray *ta, StringView src)
{
    da_reset(ta);
    if (!ta->data) da_init(ta);

    size_t i = 0;
    while (i < src.len)
    {
        TokenKind kind = token_kind_get(src, i);

        switch (kind)
        {
            case TOK_INVALID:
                da_reset(ta);
                da_append(ta, token_create(kind, SV(""), i));
                return false;

            case TOK_SPACE:
                i++;
                continue;

            case TOK_DIGIT:
            case TOK_ALPHA:
                size_t start = i;
                for (;;)
                {
                    TokenKind new_kind = token_kind_get(src, i);
                    if (new_kind == TOK_ALPHA)
                        kind = TOK_ALNUM;
                    else if (new_kind != TOK_DIGIT)
                        break;
                    i++;
                }

                StringView v = sv_slice(src, .from=start, .to=i);
                da_append(ta, token_create(kind, v, start));
                break;

            case TOK_EQ:
            case TOK_NEQ:
            case TOK_LEQ:
            case TOK_GEQ:
                da_append(ta, token_create(kind, SV(""), i));
                i += 2;
                break;

            default:
                da_append(ta, token_create(kind, SV(""), i));
                i++;
                break;
        }
    }

    da_append(ta, token_create(TOK_EOF, SV(""), i));
    return true;
}
