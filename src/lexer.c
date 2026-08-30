#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

const char *tk_to_str[] = {TOKENS(AS_STR)};

// Get the kind of token.
static TokenKind token_kind_get(StringView src)
{
    char c = src.data[0];
    char next = src.len > 1 ? src.data[1] : '\0';

    if (isdigit(c)) return TOK_DIGIT;
    if (isalpha(c)) return TOK_ALPHA;
    if (isspace(c)) return TOK_SPACE;

    switch (c)
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

        case '_': return TOK_UNDER;
        case '@': return TOK_AT;

        case '=':
            return next == '=' ? TOK_EQ : TOK_ASSIGN;
        case '!':
            if (next != '=') break;
            return TOK_NEQ;

        case '<':
        case '>':
            if (next == '=')
                return c == '<' ? TOK_LEQ : TOK_GEQ;
            return c == '<' ? TOK_LT : TOK_GT;
    }

    return TOK_INVALID;
}

static size_t token_len(TokenKind kind)
{
    switch (kind)
    {
        case TOK_EQ:
        case TOK_NEQ:
        case TOK_LEQ:
        case TOK_GEQ:
            return 2;

        default:
            return 1;
    }
}

// Create a token.
static Token token_create(TokenKind kind, StringView value, size_t pos)
{
    return (Token){.value=value, .pos=pos, .kind=kind};
}

// Construct a number token and return the length.
static size_t build_number_token(TokenArray *ta, StringView src, size_t pos)
{
    String sb;
    str_init(&sb);
    TokenKind kind = TOK_DIGIT;

    size_t i = 0;
    for (; i < src.len; i++)
    {
        bool end = false;
        switch (token_kind_get(sv_slice(src, .from=i)))
        {
            case TOK_ALPHA:
                kind = TOK_ALNUM;
                // fallthrough
            case TOK_DIGIT:
                str_append(&sb, src.data[i]);
                // fallthrough
            case TOK_UNDER:
                break;

            default:
                end = true;
                break;
        }
        if (end) break;
    }

    da_append(ta, token_create(kind, SV(sb), pos));
    // sb leaked here
    return i;
}

// Construct an identifier and return the length.
static size_t build_id_token(TokenArray *ta, StringView src, size_t pos)
{
    size_t i = 1;
    for (; i < src.len; i++)
    {
        bool end = false;
        switch (token_kind_get(sv_slice(src, .from=i)))
        {
            case TOK_ALPHA:
            case TOK_DIGIT:
            case TOK_UNDER:
                break;

            default:
                end = true;
                break;
        }
        if (end) break;
    }
    src = sv_slice(src, .to=i);

    TokenKind kind = TOK_ID;

    if (sv_equal(src, "@true"))
        kind = TOK_TRUE;
    else if (sv_equal(src, "@false"))
        kind = TOK_FALSE;

    da_append(ta, token_create(kind, src, pos));
    return i;
}

// Tokenize the source.
bool tokenize(TokenArray *ta, StringView src)
{
    da_reset(ta);
    if (!ta->data) da_init(ta);

    size_t i = 0;
    while (i < src.len)
    {
        TokenKind kind = token_kind_get(sv_slice(src, .from=i));

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
                i += build_number_token(ta, sv_slice(src, .from=i), i);
                break;

            case TOK_AT:
                i += build_id_token(ta, sv_slice(src, .from=i), i);
                break;

            default:
                size_t len = token_len(kind);
                da_append(ta, token_create(kind, sv_slice(src, .from=i, .to=i+len), i));
                i += len;
                break;
        }
    }

    da_append(ta, token_create(TOK_EOF, SV(""), i));
    return true;
}
