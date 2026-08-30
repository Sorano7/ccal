#ifndef LEXER_H
#define LEXER_H

#include <stddef.h>
#include "cut.h"

#define TOKENS(X) \
    X(TOK_EOF,     "EOF") \
    X(TOK_SPACE,   " ") \
    X(TOK_INVALID, "<invalid>") \
\
    X(TOK_ALPHA,   "alphabets") \
    X(TOK_DIGIT,   "digits") \
    X(TOK_ALNUM,   "alphanumerics") \
\
    X(TOK_PLUS,    "+") \
    X(TOK_MINUS,   "-") \
    X(TOK_STAR,    "*") \
    X(TOK_SLASH,   "/") \
    X(TOK_CARET,   "^") \
\
    X(TOK_GT,      ">") \
    X(TOK_GEQ,     ">=") \
    X(TOK_LT,      "<") \
    X(TOK_LEQ,     "<=") \
\
    X(TOK_EQ,      "==") \
    X(TOK_NEQ,     "!=") \
\
    X(TOK_DOT,     ".") \
    X(TOK_COMMA,   ",") \
    X(TOK_HASH,    "#") \
    X(TOK_UNDER,   "_") \
\
    X(TOK_LBRAC,   "[") \
    X(TOK_RBRAC,   "]") \
    X(TOK_LPAREN,  "(") \
    X(TOK_RPAREN,  ")") \
\
    X(TOK_AT,      "@") \
    X(TOK_ID,      "identifier") \
    X(TOK_ASSIGN,  "=") \
\
    X(TOK_TRUE,    "@true") \
    X(TOK_FALSE,   "@false") \
    X(TOK_LAST,    "@@")

#define AS_ENUM(name, _) name,
#define AS_STR(name, s)  [name] = (s),

// Kinds of a token.
typedef enum
{
    TOKENS(AS_ENUM)
} TokenKind;

// Lookup the string representation of the token kind.
extern const char *tk_to_str[];

// A token.
typedef struct
{
    StringView value;
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
bool tokenize(TokenArray *ta, StringView src);

#endif
