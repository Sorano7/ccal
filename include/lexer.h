#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>

// Kinds of a token.
typedef enum
{
    TOK_SPACE,
    TOK_INVALID,
    TOK_NUMBER,
    TOK_PLUS,
    TOK_MINUS,
    TOK_STAR,
    TOK_SLASH,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_EOF,
} TokenKind;

// A token.
typedef struct
{
    const char *data;
    size_t len;
    size_t start;
    TokenKind kind;
} Token;

// Lexer for tokenizing the source.
typedef struct
{
    const char *src;
    Token *tokens;
    size_t len;
    size_t cap;
} Lexer;


TokenKind token_kind_get(char c);

bool lexer_init(Lexer *l);
void lexer_reset(Lexer *l);
bool lexer_push(Lexer *l, Token tok);
void lexer_print(Lexer *l);
bool tokenize(Lexer *l, const char *src);

#endif
