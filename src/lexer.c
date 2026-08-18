#include "lexer.h"
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>

// Convert a token kind to string.
static const char *token_kind_str[] = {
    [TOK_SPACE]   = "SPACE",
    [TOK_INVALID] = "INVALID",
    [TOK_NUMBER]  = "NUMBER",
    [TOK_PLUS]    = "PLUS",
    [TOK_MINUS]   = "MINUS",
    [TOK_STAR]    = "STAR",
    [TOK_SLASH]   = "SLASH",
    [TOK_LPAREN]  = "LPAREN",
    [TOK_RPAREN]  = "RPAREN",
    [TOK_EOF]     = "EOF",
};

// Get the kind of token based on the first character.
TokenKind token_kind_get(char c)
{
    switch (c)
    {
        case '+': return TOK_PLUS;
        case '-': return TOK_MINUS;
        case '*': return TOK_STAR;
        case '/': return TOK_SLASH;
        case '(': return TOK_LPAREN;
        case ')': return TOK_RPAREN;
    }

    if (isalpha(c) || isdigit(c))
        return TOK_NUMBER;

    if (isspace(c)) return TOK_SPACE;

    return TOK_INVALID;
}


// Initialize a lexer.
bool lexer_init(Lexer *l)
{
    l->src = NULL;
    l->cap = 128;
    l->len = 0;
    l->tokens = malloc(sizeof(Token) * l->cap);
    if (!l->tokens) return false;
    return true;
}

// Reset a lexer.
void lexer_reset(Lexer *l)
{
    l->src = NULL;
    l->len = 0;
}

// Push a new token to the lexer.
bool lexer_push(Lexer *l, Token tok)
{
    if (l->len + 1 > l->cap)
    {
        size_t new_cap = l->cap * 2;
        Token *new_data = realloc(l->tokens, new_cap);
        if (!new_data) return false;
        l->tokens = new_data;
        l->cap = new_cap;
    }
    l->tokens[l->len++] = tok;
    return true;
}

// Print the tokens in the lexer.
void lexer_print(Lexer *l)
{
    for (size_t i = 0; i < l->len; i++)
    {
        Token tok = l->tokens[i];
        printf("%02zu: %s(", i, token_kind_str[tok.kind]);
        if (tok.len > 0)
            printf("%.*s", (int)tok.len, tok.data);
        printf(")\n");
    }
}

// Tokenize the source. src will be referenced in the lexer.
// Return false if an invalid token is encountered.
bool tokenize(Lexer *l, const char *src)
{
    l->src = src;
    size_t i = 0;
    while (l->src[i] != '\0')
    {
        TokenKind kind = token_kind_get(l->src[i]);
        if (kind == TOK_INVALID) return false;

        if (kind == TOK_SPACE)
        {
            i++;
            continue;
        }

        if (kind == TOK_NUMBER)
        {
            size_t start = i;
            while (token_kind_get(l->src[++i]) == kind);
            lexer_push(l, (Token){l->src+start, i - start, i, kind});
        }
        else
        {
            lexer_push(l, (Token){l->src+i, 1, i, kind});
            i++;
        }
    }
    lexer_push(l, (Token){NULL, 0, i, TOK_EOF});
    return true;
}
