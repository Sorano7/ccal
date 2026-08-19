#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

const char *tk_to_str[] = {TOKENS(AS_STR)};

#define NEXT_IS_THEN(src, i, c, tk) do { \
    if ((src)[(i)+1] == (c)) return (tk); \
} while (0)

// Get the kind of token at the pointer.
static TokenKind token_kind_get(const char *src, size_t i)
{
    if (isdigit(src[i])) return TOK_DIGIT;
    if (isalpha(src[i])) return TOK_ALPHA;
    if (isspace(src[i])) return TOK_SPACE;

    switch (src[i])
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

// Initialize a token array or reset if is already allocated.
static void ta_init_or_reset(TokenArray *ta)
{
    assert(ta);
    ta->len = 0;

    if (!ta->data) 
    {
        ta->cap = 128;
        ta->data = malloc(sizeof(Token) * ta->cap);
    }
    assert(ta->data);
}

// Push a new token to the lexer.
static bool ta_push(TokenArray *ta, Token tok)
{
    if (ta->len + 1 > ta->cap)
    {
        size_t new_cap = ta->cap * 2;
        Token *new_data = realloc(ta->data, new_cap * sizeof(Token));
        if (!new_data) return false;
        ta->data = new_data;
        ta->cap = new_cap;
    }
    ta->data[ta->len++] = tok;
    return true;
}

// Free a token array.
void ta_free(TokenArray *ta)
{
    if (!ta) return;
    ta->cap = 0;
    ta->len = 0;
    if (ta->data)
        free(ta->data);
    ta->data = NULL;
}

// Create a token.
static Token token_create(TokenKind kind, size_t pos)
{
    return (Token){.data=NULL, .len=0, .pos=pos, .kind=kind};
}

// Return a token with additional data.
static Token token_with_data(Token tok, const char *data, size_t len)
{
    tok.data = data;
    tok.len = len;
    return tok;
}

// Tokenize the source.
bool tokenize(TokenArray *ta, const char *src)
{
    ta_init_or_reset(ta);

    size_t i = 0;
    while (src[i] != '\0')
    {
        TokenKind kind = token_kind_get(src, i);

        switch (kind)
        {
            case TOK_INVALID:
                ta_init_or_reset(ta);
                ta_push(ta, token_create(kind, i));
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

                Token tok = token_create(kind, start);
                ta_push(ta, token_with_data(tok, src+start, i-start));
                break;

            case TOK_EQ:
            case TOK_NEQ:
            case TOK_LEQ:
            case TOK_GEQ:
                ta_push(ta, token_create(kind, i));
                i += 2;
                break;

            default:
                ta_push(ta, token_create(kind, i));
                i++;
                break;
        }
    }

    ta_push(ta, token_create(TOK_EOF, i));
    return true;
}
