#include "lexer.h"
#include <ctype.h>

// Convert a token to an operator.
Operator token_to_op(Token t)
{
    switch (t.kind)
    {
        case TOK_EQ:     return OP_EQ;
        case TOK_NEQ:    return OP_NEQ;

        case TOK_LT:     return OP_LT;
        case TOK_LEQ:    return OP_LEQ;
        case TOK_GT:     return OP_GT;
        case TOK_GEQ:    return OP_GEQ;

        case TOK_PLUS:   return OP_ADD;
        case TOK_MINUS:  return OP_SUB;
        case TOK_STAR:   return OP_MUL;
        case TOK_SLASH:  return OP_DIV;
        case TOK_CARET:  return OP_POW;

        case TOK_ASSIGN: return OP_ASSIGN;
        case TOK_DOLLAR: return OP_PIPE;

        default:         return OP_NIL;
    }
}

// Free a list of tokens and all of its contents.
void token_list_free(TokenList *tl)
{
    DA_FOR(tl, i)
    {
        str_free(&da_at(tl, i).value);
    }
    da_free(tl);
}

const char *tk_to_str[] = {TOKENS(AS_STR)};

// Get the kind of token.
static TokenKind token_kind_get(StringView src)
{
    char c = src.data[0];
    char next = src.len > 1 ? src.data[1] : '\0';

    switch (c)
    {
        case '+':  return TOK_PLUS;
        case '-':  return TOK_MINUS;
        case '*':  return TOK_STAR;
        case '/':  return TOK_SLASH;
        case '^':  return TOK_CARET;

        case '.':  return TOK_DOT;
        case ',':  return TOK_COMMA;
        case '#':  return TOK_HASH;

        case '[':  return TOK_LBRAC;
        case ']':  return TOK_RBRAC;
        case '(':  return TOK_LPAREN;
        case ')':  return TOK_RPAREN;

        case '_':  return TOK_UNDER;
        case ':':  return TOK_COLON;
        case '$':  return TOK_DOLLAR;
        case '?':  return TOK_QUESTION;
        case '|':  return TOK_BAR;
        case '\'': return TOK_SQUOTE;
        case '`':  return TOK_BACKTICK;

        case ';':  return TOK_SEMICOLON;
        case '\n': return TOK_NEWLINE;

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

    if (isdigit(c)) return TOK_DIGIT;
    if (isalpha(c)) return TOK_ALPHA;
    if (isspace(c)) return TOK_SPACE;

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
static inline void token_init(Token *t, TokenKind kind, StringView value, Span span)
{
    t->kind = kind;
    str_init_with(&t->value, value);
    t->span = span;
    t->ws_prefix = false;
    t->ws_suffix = false;
}

static inline void token_errorf(Token *t, Span span, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    token_init(t, TOK_ERROR, SV(""), span);
    str_appendvf(&t->value, fmt, args);

    va_end(args);
}

#define SRC sv_slice(src, .from=i)

// Construct a number token.
static void build_number_token(Token *t, StringView src, size_t *pos)
{
    TokenKind kind = TOK_DIGIT;
    size_t i = 0;
    bool end = false;

    for (; i < src.len; i++)
    {
        switch (token_kind_get(SRC))
        {
            case TOK_ALPHA:
                kind = TOK_ALNUM;
                // fallthrough
            case TOK_DIGIT:
            case TOK_UNDER:
                break;

            default:
                end = true;
                break;
        }
        if (end) break;
    }

    token_init(t, kind, sv_slice(src, .to=i), (Span){*pos, *pos+i});
    *pos += i;
}

// Construct an identifier token.
static bool build_id_token(Token *t, StringView src, size_t *pos)
{
    size_t i = 1;
    bool end = false;

    for (; i < src.len; i++)
    {
        switch (token_kind_get(SRC))
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

    if (i == 1)
    {
        token_errorf(t, (Span){*pos, *pos+i}, "Empty indentifier");
        return false;
    }

    token_init(t, TOK_ID, sv_slice(src, .from=1, .to=i), (Span){*pos, *pos+i});
    *pos += i;
    return true;
}

// Construct a infix identifier token.
static bool build_infix_id_token(Token *t, StringView src, size_t *pos)
{
    size_t i = 1;
    bool end = false;
    bool closed = false;

    for (; i < src.len; i++)
    {
        switch (token_kind_get(SRC))
        {
            case TOK_ALPHA:
            case TOK_DIGIT:
            case TOK_UNDER:
                break;

            case TOK_BACKTICK:
                i++;
                closed = true;
                break;

            default:
                end = true;
                break;
        }
        if (closed || end) break;
    }

    if (!closed)
    {
        token_errorf(t, (Span){*pos+i, *pos+i+1}, "Expected '`'");
        return false;
    }

    if (i-1 < 2)
    {
        token_errorf(t, (Span){*pos, *pos+i}, "Empty infix indentifier");
        return false;
    }

    token_init(t, TOK_INFIX_ID, sv_slice(src, .from=1, .to=i-1), (Span){*pos, *pos+i});
    *pos += i;
    return true;
}

// Tokenize the source.
bool tokenize(TokenList *tl, StringView src)
{
    bool pending_space = false;

    size_t i = 0;
    Token t = {0};

    while (i < src.len)
    {
        TokenKind kind = token_kind_get(SRC);
        size_t len = token_len(kind);
        Span span = {i, i+len};
        bool ok = true;

        if (kind == TOK_SPACE)
        {
            while (i < src.len)
            {
                kind = token_kind_get(SRC);
                if (kind != TOK_SPACE) break;
                i++;
            }
            pending_space = true;

            if (tl->len > 0)
                da_last(tl).ws_suffix = true;

            continue;
        }
        switch (kind)
        {
            case TOK_INVALID:
                token_errorf(&t, span, "Invalid token");
                da_append(tl, t);
                return false;

            case TOK_SPACE:
                break;

            case TOK_DIGIT:
            case TOK_ALPHA:
                build_number_token(&t, SRC, &i);
                da_append(tl, t);
                break;

            case TOK_SQUOTE:
                ok = build_id_token(&t, SRC, &i);
                da_append(tl, t);
                if (!ok) return false;
                break;

            case TOK_BACKTICK:
                ok = build_infix_id_token(&t, SRC, &i);
                da_append(tl, t);
                if (!ok) return false;
                break;

            default:
                token_init(&t, kind, sv_slice(src, .from=i, .to=i+len), span);
                da_append(tl, t);
                i += len;
                break;
        }

        if (pending_space) {
            if (tl->len > 0)
                da_last(tl).ws_prefix = true;
            pending_space = false;
        }
    }

    token_init(&t, TOK_EOF, SV(" "), (Span){i, i+1});
    da_append(tl, t);
    return true;
}

