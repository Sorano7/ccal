#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

// Convert a token kind to string.
static const char *token_kind_str[] = {
    [TOK_EOF]     = "EOF",
    [TOK_SPACE]   = "SPACE",
    [TOK_INVALID] = "INVALID",

    [TOK_ALPHA]   = "ALPHA",
    [TOK_DIGIT]   = "DIGIT",
    [TOK_ALNUM]   = "ALNUM",

    [TOK_PLUS]    = "PLUS",
    [TOK_MINUS]   = "MINUS",
    [TOK_STAR]    = "STAR",
    [TOK_SLASH]   = "SLASH",

    [TOK_DOT]     = "DOT",
    [TOK_COMMA]   = "COMMA",

    [TOK_LBRAC]   = "LBRAC",
    [TOK_RBRAC]   = "RBRAC",
    [TOK_LPAREN]  = "LPAREN",
    [TOK_RPAREN]  = "RPAREN",
};

// Get the kind of token based on the first character.
static TokenKind token_kind_get(char c)
{
    switch (c)
    {
        case '+': return TOK_PLUS;
        case '-': return TOK_MINUS;
        case '*': return TOK_STAR;
        case '/': return TOK_SLASH;

        case '.': return TOK_DOT;
        case ',': return TOK_COMMA;

        case '[': return TOK_LBRAC;
        case ']': return TOK_RBRAC;
        case '(': return TOK_LPAREN;
        case ')': return TOK_RPAREN;
    }

    if (isdigit(c)) return TOK_DIGIT;
    if (isalpha(c)) return TOK_ALPHA;
    if (isspace(c)) return TOK_SPACE;

    return TOK_INVALID;
}

// Initialize a token array or reset if is already allocated.
static bool ta_init_or_reset(TokenArray *ta)
{
    assert(ta);
    ta->cap = 128;
    ta->len = 0;

    if (!ta->data) ta->data = malloc(sizeof(Token) * ta->cap);
    if (!ta->data) return false;
    return true;
}

// Push a new token to the lexer.
static bool ta_push(TokenArray *ta, Token tok)
{
    if (ta->len + 1 > ta->cap)
    {
        size_t new_cap = ta->cap * 2;
        Token *new_data = realloc(ta->data, new_cap);
        if (!new_data) return false;
        ta->data = new_data;
        ta->cap = new_cap;
    }
    ta->data[ta->len++] = tok;
    return true;
}

void ta_free(TokenArray *ta)
{
    if (!ta) return;
    ta->cap = 0;
    ta->len = 0;
    if (ta->data)
        free(ta->data);
    ta->data = NULL;
}

// Print the tokens in the lexer.
void ta_print(const TokenArray *ta)
{
    for (size_t i = 0; i < ta->len; i++)
    {
        Token tok = ta->data[i];
        printf("%02zu: %s", i, token_kind_str[tok.kind]);

        if (tok.len > 0)
            printf("(%.*s)", (int)tok.len, tok.data);

        printf("\n");
    }
}

static Token token_create(TokenKind kind, size_t pos)
{
    return (Token){.data=NULL, .len=0, .pos=pos, .kind=kind};
}

static Token token_with_data(Token tok, const char *data, size_t len)
{
    tok.data = data;
    tok.len = len;
    return tok;
}

// Tokenize the source.
bool tokenize(TokenArray *ta, const char *src)
{
    if (!ta_init_or_reset(ta)) return false;

    size_t i = 0;
    while (src[i] != '\0')
    {
        TokenKind kind = token_kind_get(src[i]);
        if (kind == TOK_INVALID) return false;

        if (kind == TOK_SPACE)
        {
            i++;
            continue;
        }

        if (kind == TOK_DIGIT || kind == TOK_ALPHA)
        {
            size_t start = i;
            for (;;)
            {
                TokenKind new_kind = token_kind_get(src[i]);
                switch (new_kind)
                {
                    case TOK_ALPHA:
                        kind = TOK_ALNUM;
                        // fallthrough
                    case TOK_DIGIT:
                        i++;
                        continue;

                    default:
                        break;
                }
                break;
            }

            Token tok = token_create(kind, i);
            ta_push(ta, token_with_data(tok, src+start, i-start));
        }
        else
        {
            ta_push(ta, token_create(kind, i));
            i++;
        }
    }
    ta_push(ta, token_create(TOK_EOF, i));
    return true;
}
