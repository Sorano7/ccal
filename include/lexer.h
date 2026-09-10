#ifndef LEXER_H
#define LEXER_H

#include "cut.h"
#include "ast.h"

#define TOKENS(X) \
    X(TOK_EOF,       "EOF") \
    X(TOK_SPACE,     " ") \
    X(TOK_NEWLINE,   "\n") \
    X(TOK_SEMICOLON, ";") \
    X(TOK_INVALID,   "<invalid>") \
    X(TOK_ERROR,     "<error>") \
\
    X(TOK_ALPHA,     "alphabets") \
    X(TOK_DIGIT,     "digits") \
    X(TOK_ALNUM,     "alphanumerics") \
    X(TOK_ID,        "identifier") \
    X(TOK_INFIX_ID,  "infix identifier") \
\
    X(TOK_PLUS,      "+") \
    X(TOK_MINUS,     "-") \
    X(TOK_STAR,      "*") \
    X(TOK_SLASH,     "/") \
    X(TOK_CARET,     "^") \
\
    X(TOK_GT,        ">") \
    X(TOK_GEQ,       ">=") \
    X(TOK_LT,        "<") \
    X(TOK_LEQ,       "<=") \
\
    X(TOK_EQ,        "==") \
    X(TOK_NEQ,       "!=") \
\
    X(TOK_DOT,       ".") \
    X(TOK_COMMA,     ",") \
    X(TOK_HASH,      "#") \
    X(TOK_UNDER,     "_") \
\
    X(TOK_LBRAC,     "[") \
    X(TOK_RBRAC,     "]") \
    X(TOK_LPAREN,    "(") \
    X(TOK_RPAREN,    ")") \
\
    X(TOK_SQUOTE,    "'") \
    X(TOK_ASSIGN,    "=") \
\
    X(TOK_COLON,     ":") \
    X(TOK_DOLLAR,    "$") \
    X(TOK_QUESTION,  "?") \
    X(TOK_BAR,       "|") \
    X(TOK_BACKTICK,  "`")

#define AS_ENUM(name, _) name,
#define AS_STR(name, s)  [name] = (s),

// Kinds of a token.
typedef enum
{
    TOKENS(AS_ENUM)
} TokenKind;

extern const char *tk_to_str[];

// A token.
typedef struct
{
    String value;
    Span span;
    TokenKind kind;

    bool ws_prefix;
    bool ws_suffix;
} Token;

typedef struct
{
    Token *data;
    size_t len;
    size_t cap;
} TokenList;

Operator token_to_op(Token t);

void token_list_free(TokenList *ta);

bool tokenize(TokenList *ta, StringView src);

#endif
