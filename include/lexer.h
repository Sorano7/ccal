#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

// Kinds of a token.
typedef enum
{
    TOK_EOF,
    TOK_SPACE,
    TOK_INVALID,

    TOK_ALPHA,
    TOK_DIGIT,
    TOK_ALNUM,

    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,

    TOK_DOT,
    TOK_COMMA,

    TOK_LBRAC,
    TOK_RBRAC,
    TOK_LPAREN,
    TOK_RPAREN,
} TokenKind;

// A token.
typedef struct
{
    const char *data;
    size_t len;
    size_t pos;
    TokenKind kind;
} Token;

typedef struct
{
    Token *data;
    size_t len;
    size_t cap;
} TokenArray;

void ta_print(const TokenArray *ta);
void ta_free(TokenArray *ta);

bool tokenize(TokenArray *ta, const char *src);

#endif
