#include "parser.h"
#include "number.h"

#include <errno.h>
#include <ctype.h>


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
} TokenArray;

static void token_array_free(TokenArray *ta)
{
    DA_FOR(ta, i)
    {
        str_free(&da_at(ta, i).value);
    }
    da_free(ta);
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
static bool tokenize(TokenArray *ta, StringView src)
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

            if (ta->len > 0)
                da_last(ta).ws_suffix = true;

            continue;
        }
        switch (kind)
        {
            case TOK_INVALID:
                token_errorf(&t, span, "Invalid token");
                da_append(ta, t);
                return false;

            case TOK_SPACE:
                break;

            case TOK_DIGIT:
            case TOK_ALPHA:
                build_number_token(&t, SRC, &i);
                da_append(ta, t);
                break;

            case TOK_SQUOTE:
                ok = build_id_token(&t, SRC, &i);
                da_append(ta, t);
                if (!ok) return false;
                break;

            case TOK_BACKTICK:
                ok = build_infix_id_token(&t, SRC, &i);
                da_append(ta, t);
                if (!ok) return false;
                break;

            default:
                token_init(&t, kind, sv_slice(src, .from=i, .to=i+len), span);
                da_append(ta, t);
                i += len;
                break;
        }

        if (pending_space) {
            if (ta->len > 0)
                da_last(ta).ws_prefix = true;
            pending_space = false;
        }
    }

    token_init(&t, TOK_EOF, SV(" "), (Span){i, i+1});
    da_append(ta, t);
    return true;
}

// Operator precedence levels.
typedef enum
{
    PREC_PRIMARY,

    PREC_COND,

    PREC_ASSIGN,

    PREC_EQUALITY,
    PREC_COMPARISON,

    PREC_SUM,
    PREC_PRODUCT,
    PREC_POWER,

    PREC_APPLY,

    PREC_PREFIX,
    PREC_BASE,
} OpPrec;

// Get the precedence of the token.
static OpPrec token_prec(Token t)
{
    switch (t.kind)
    {
        case TOK_QUESTION:
            return PREC_COND;

        case TOK_EQ:
        case TOK_NEQ:
            return PREC_EQUALITY;

        case TOK_LT:
        case TOK_LEQ:
        case TOK_GT:
        case TOK_GEQ:
            return PREC_COMPARISON;

        case TOK_PLUS:
        case TOK_MINUS:
            return PREC_SUM;

        case TOK_STAR:
        case TOK_SLASH:
            return PREC_PRODUCT;

        case TOK_CARET:
            return PREC_POWER;

        case TOK_ASSIGN:
            return PREC_ASSIGN;

        case TOK_DOLLAR:
        case TOK_INFIX_ID:
            return PREC_APPLY;

        default:
            return PREC_PRIMARY;
    }
}

// Convert a token to an operator.
static Operator token_to_op(Token t)
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

// Return whether the operator is right associative.
static bool is_right_associative(Operator op)
{
    switch (op)
    {
        case OP_EQ:
        case OP_NEQ:
        case OP_LT:
        case OP_LEQ:
        case OP_GT:
        case OP_GEQ:
        case OP_POW:
        case OP_NEG:
        case OP_ASSIGN:
        case OP_PIPE:
            return true;

        default:
            return false;
    }
}

typedef struct
{
    TokenArray *ta;
    size_t pos;
    unsigned long base;
} Parser;

#define AT_OR_LAST(p, i) (i) < (p)->ta->len ? (i) : (p)->ta->len-1

#define peek(p, n)  (p)->ta->data[AT_OR_LAST((p), (p)->pos + (n))]
#define token(p)    peek(p, 0)
#define tprec(p)    token_prec(token(p))
#define tkind(p)    token(p).kind
#define tspan(p)    token(p).span
#define is_alnum(p) (tkind(p) == TOK_ALNUM || tkind(p) == TOK_DIGIT)
#define is_dlist(p) (tkind(p) == TOK_LBRAC)
#define is_sexpr(p) (is_alnum(p) || is_dlist(p) || tkind(p) == TOK_LPAREN)

#define CONSUME_EXPECT(p, k) do { \
    if (tkind(p) != k) \
        return expr_err(tspan(p), "Expected '%s'", tk_to_str[k]); \
    (p)->pos++; \
} while (0)

// Return if the next expression is nud.
static inline bool is_nud(Parser *p)
{
    switch (tkind(p))
    {
        case TOK_DIGIT:
        case TOK_LPAREN:
        case TOK_ALNUM:
        case TOK_ID:
        case TOK_LBRAC:
        case TOK_UNDER:
        case TOK_COLON:
            return true;

        case TOK_MINUS:
            return !token(p).ws_suffix;

        default:
            return false;
    }
}

static Expr *parse_expr(Parser *p, int prec);

// Convert a token to an unsigned long value.
// t must be TOK_DIGIT.
static bool token_to_ul(Token t, unsigned long *out)
{
    SV_TO_CSTR(t.value, buffer);
    errno = 0;

    char *end_ptr = NULL;
    *out = strtoul(buffer, &end_ptr, 10);
    return errno != ERANGE;
}

// Parse an alphanumeric number part, return error.
static Expr *parse_number_part_alnum(Parser *p, DigitArray *ds, bool has_base, Span *out)
{
    Token t = token(p);
    Span s = t.span;

    if (!is_alnum(p))
        return expr_err(s, "Expected alphanumerics");

    StringView src = SV(t.value);
    if (has_base) sv_shift(&src, 2);

    DigitResult res = digits_from_alnum(ds, src, p->base);
    s.from += res.pos;
    s.to = s.from+1;

    switch (res.kind)
    {
        case DIGIT_INVALID:
            return expr_err(s, "Not a digit");
        case DIGIT_OOB:
            return expr_err(s, "Digit out of bounds for base %lu", p->base);
        case DIGIT_BASE_TOO_LARGE:
            return expr_err(t.span, "Base too large for alphanumeric spelling");
        default:
            break;
    }
    p->pos++;
    out->to = t.span.to;
    return NULL;
}

// Parse a digit list number part, return error.
static Expr *parse_number_part_dlist(Parser *p, DigitArray *ds, Span *out)
{
    Token t = token(p);
    Span s = t.span;

    if (!is_dlist(p))
        return expr_err(s, "Expected digit list");

    CONSUME_EXPECT(p, TOK_LBRAC);
        for (;;)
        {
            t = token(p);
            if (tkind(p) != TOK_DIGIT)
                return expr_err(t.span, "Expected numeric value as digit");

            unsigned long val;
            if (!token_to_ul(t, &val) || val >= p->base)
                return expr_err(t.span, "Digit out of bounds");

            da_append(ds, val);
            p->pos++;

            if (tkind(p) == TOK_RBRAC) break;
            CONSUME_EXPECT(p, TOK_COMMA);
        }
        out->to = tspan(p).to;
    CONSUME_EXPECT(p, TOK_RBRAC);
    return NULL;
}

// Parse a number part (I, N, or R) into a sequence of digits.
// Ensure the number is in the same format as fmt.
static Expr *parse_number_part(Parser *p, DigitArray *ds, DigitFormat fmt, bool has_base, Span *out)
{
    switch (fmt)
    {
        case DIGIT_FMT_ALNUM:
            return parse_number_part_alnum(p, ds, has_base, out);

        case DIGIT_FMT_LIST:
            return parse_number_part_dlist(p, ds, out);

        default:
            return expr_err(tspan(p), "Expected number");
    }
}

// Try parsing a base prefix in the form of a leading zero.
static bool try_parse_base_prefix(Token *t, unsigned long *base)
{
    if (t->kind != TOK_ALNUM) return false;

    if (sv_startswith(SV(t->value), SV("0x")))
        *base = 16;
    else if (sv_startswith(SV(t->value), SV("0b")))
        *base = 2;
    else if (sv_startswith(SV(t->value), SV("0o")))
        *base = 8;
    else if (sv_startswith(SV(t->value), SV("0d")))
        *base = 10;
    else
        return false;
    return true;
}

// Parse a number literal in the form of I.N(R).
static Expr *parse_number(Parser *p, DigitFormat fmt)
{
    Expr *e = NULL;
    Span s = {0};

    Literal lit;
    literal_init(&lit);

    unsigned long prev_base = p->base;
    bool has_base = try_parse_base_prefix(&token(p), &p->base);

    if ((e = parse_number_part(p, &lit.I, fmt, has_base, &s)))
        goto cleanup;

    if (tkind(p) != TOK_DOT)
        goto eval;
    p->pos++;

    if (tkind(p) != TOK_LPAREN)
    {
        if ((e = parse_number_part(p, &lit.N, fmt, false, &s)))
            goto cleanup;
    }

    if (tkind(p) == TOK_LPAREN)
    {
        p->pos++;
        if ((e = parse_number_part(p, &lit.R, fmt, false, &s)))
            goto cleanup;

        if (tkind(p) != TOK_RPAREN)
        {
            e = expr_err(tspan(p), "Expected ')'");
            goto cleanup;
        }
        p->pos++;
    }

eval:
    e = expr_number(s);
    literal_to_mpq(&lit, p->base, e->as.number);

cleanup:
    literal_free(&lit);
    p->base = prev_base;
    return e;
}

// Parse a single expression in the base denoted by the tag.
static Expr *parse_base_tag(Parser *p)
{
    Token t = token(p);
    Span s = t.span;

    CONSUME_EXPECT(p, TOK_DIGIT);
    CONSUME_EXPECT(p, TOK_HASH);

    unsigned long prev_base = p->base;

    if (!token_to_ul(t, &p->base))
        return expr_err(s, "Base too large");
    if (p->base <= 1)
        return expr_err(s, "Base must be at least 2");
    if (!is_sexpr(p))
        return expr_err(tspan(p), "Base prefix must precede expression");

    Expr *e = parse_expr(p, PREC_BASE);
    p->base = prev_base;
    return e;
}

// Parse a group expression.
static Expr *parse_group(Parser *p)
{
    CONSUME_EXPECT(p, TOK_LPAREN);
    Expr *e = parse_expr(p, PREC_PRIMARY);
    if (is_error(e)) return e;
    CONSUME_EXPECT(p, TOK_RPAREN);
    return e;
}

// Parse a negation expression.
static Expr *parse_neg(Parser *p)
{
    Span s = tspan(p);
    CONSUME_EXPECT(p, TOK_MINUS);

    if (!is_sexpr(p))
            return expr_err(tspan(p), "Expected number of group");

    Expr *e = parse_expr(p, PREC_PREFIX);
    if (is_error(e)) return e;

    return expr_prefix(s, OP_NEG, e);
}

// Parse an identifier
static Expr *parse_ident(Parser *p)
{
    Token t = token(p);
    CONSUME_EXPECT(p, TOK_ID);
    return expr_id(t.span, SV(t.value));
}

// Parse a lambda expression.
static Expr *parse_lambda(Parser *p)
{
    Expr *id = parse_ident(p);
    if (is_error(id)) return id;

    CONSUME_EXPECT(p, TOK_COLON);

    Expr *body = parse_expr(p, PREC_PRIMARY);
    if (is_error(body)) return body;

    return expr_lambda(id, body);
}

// Parse a null denotation expression.
static Expr *parse_nud(Parser *p)
{
    if (peek(p, 1).kind == TOK_HASH)
        return parse_base_tag(p);

    if (peek(p, 1).kind == TOK_COLON)
        return parse_lambda(p);

    if (is_alnum(p))
        return parse_number(p, DIGIT_FMT_ALNUM);

    if (is_dlist(p))
        return parse_number(p, DIGIT_FMT_LIST);

    switch (tkind(p))
    {
        case TOK_ID:     return parse_ident(p);
        case TOK_LPAREN: return parse_group(p);
        case TOK_MINUS:  return parse_neg(p);
        default:         return expr_err(tspan(p), "Expected expression");
    }
}

// Parse an application expression.
static Expr *parse_apply(Parser *p, Expr *func)
{
    Expr *arg = parse_nud(p);
    if (is_error(arg))
    {
        expr_destroy(&func);
        return arg;
    }
    return expr_infix(func, OP_APPLY, arg);
}

// Parse a conditional expression.
static Expr *parse_cond(Parser *p, Expr *if_)
{
    CONSUME_EXPECT(p, TOK_QUESTION);

    Expr *then = parse_expr(p, PREC_PRIMARY);
    if (is_error(then)) 
    {
        expr_destroy(&if_);
        return then;
    }

    CONSUME_EXPECT(p, TOK_BAR);

    Expr *else_ = parse_expr(p, PREC_PRIMARY);
    if (is_error(else_)) 
    {
        expr_destroy(&if_);
        expr_destroy(&then);
        return else_;
    }
    return expr_cond(if_, then, else_);
}

// Parse an infix application of an identifier.
static Expr *parse_infix_apply(Parser *p, Expr *left)
{
    Expr *f = expr_id(tspan(p), SV(token(p).value));
    p->pos++;

    Expr *right = parse_expr(p, PREC_PRIMARY);
    if (is_error(right))
    {
        expr_destroy(&f);
        return right;
    }

    return expr_infix(expr_infix(f, OP_APPLY, left), OP_APPLY, right);
}

// Parse a left denotation expression
static Expr *parse_led(Parser *p, int prec, Expr *left)
{
    if (tkind(p) == TOK_QUESTION)
        return parse_cond(p, left);

    if (tkind(p) == TOK_INFIX_ID)
        return parse_infix_apply(p, left);

    Operator op = token_to_op(token(p));
    switch (op)
    {
        case OP_NIL:
            return expr_err(tspan(p), "Unknown operator");
        case OP_ASSIGN:
            if (left->kind != EXPR_IDENT)
                return expr_err(left->span, "Expected identifier");
            break;
        default:
            break;
    }
    p->pos++;

    if (is_right_associative(op))
        prec--;

    Expr *right = parse_expr(p, prec);
    if (is_error(right))
    {
        expr_destroy(&left);
        return right;
    }

    return expr_infix(left, op, right);
}

// Parse an expression.
static Expr *parse_expr(Parser *p, int prec)
{
    Expr *e = parse_nud(p);
    if (is_error(e)) return e;

    for (;;)
    {
        if (tkind(p) == TOK_EOF) break;

        if (token(p).ws_prefix && is_nud(p))
        {
            e = parse_apply(p, e);
        }
        else
        {
            if ((int)tprec(p) <= prec) break;
            e = parse_led(p, tprec(p), e);
        }
        if (is_error(e)) return e;
    }
    return e;
}

// Parse a module.
bool parse_module(StringView src, unsigned long base, Module *m)
{
    Span s = {0, 0};
    if (base == 0) base = BASE_DEFAULT;
    if (base == 1) return expr_err(s, "Base must be at least 2");

    bool ok = true;

    TokenArray ta = {0};
    da_init(&ta);

    Expr *e = NULL;

    if (!tokenize(&ta, src))
    {
        Token err = da_last(&ta);
        if (err.kind == TOK_ERROR)
        {
            e = expr_err(err.span, SV_FMT, SV_ARG(SV(err.value)));
            da_append(m, e);
        }
        ok = false;
        goto cleanup;
    }

    Parser p = {
        .ta = &ta,
        .pos = 0,
        .base = base,
    };

    for (;;)
    {
        e = parse_expr(&p, PREC_PRIMARY);
        da_append(m, e);

        if (is_error(e))
        {
            ok = false;
            goto cleanup;
        }

        TokenKind tk = tkind(&p);
        if (tk == TOK_EOF) break;

        if (tk != TOK_NEWLINE && tk != TOK_SEMICOLON)
        {
            e = expr_err(tspan(&p), "Trailing characters");
            da_append(m, e);
            ok = false;
            goto cleanup;
        }

        for (; tk == TOK_NEWLINE || tk == TOK_SEMICOLON; tk = tkind(&p))
            p.pos++;
    }

cleanup:
    token_array_free(&ta);
    return ok;
}

// Parse an expression.
Expr *parse(StringView src, unsigned long base)
{
    Module m;
    da_init(&m);

    parse_module(src, base, &m);

    Expr *e = expr_clone(da_last(&m));
    module_free(&m);
    return e;
}

