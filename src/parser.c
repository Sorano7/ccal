#include "parser.h"
#include "number.h"

#include <errno.h>
#include <ctype.h>


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
    X(TOK_COLON,   ":") \
    X(TOK_CALL,    "$")

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
        case ':': return TOK_COLON;
        case '$': return TOK_CALL;

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
static inline Token token_create(TokenKind kind, StringView value, size_t pos)
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
    TokenKind kind = TOK_ID;

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

            case TOK_AT:
                end = true;
                i++;
                break;

            default:
                end = true;
                break;
        }
        if (end) break;
}
    src = sv_slice(src, .to=i);

    da_append(ta, token_create(kind, src, pos));
    return i;
}

// Tokenize the source.
static bool tokenize(TokenArray *ta, StringView src)
{
    size_t i = 0;
    while (i < src.len)
    {
        TokenKind kind = token_kind_get(sv_slice(src, .from=i));

        switch (kind)
        {
            case TOK_INVALID:
                da_reset(ta);
                da_append(ta, token_create(kind, SV(" "), i));
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

// Operator precedence levels.
typedef enum
{
    PREC_PRIMARY,

    PREC_ASSIGN,

    PREC_EQUALITY,
    PREC_COMPARISON,

    PREC_SUM,
    PREC_PRODUCT,
    PREC_POWER,

    PREC_CALL,

    PREC_PREFIX,
    PREC_BASE,
} OpPrec;

// Get the precedence of the token.
static OpPrec token_prec(Token t)
{
    switch (t.kind)
    {
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

        case TOK_CALL:
            return PREC_CALL;

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
        case TOK_CALL:   return OP_CALL;

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
#define tspan(p)    token_span(token(p))
#define is_alnum(p) (tkind(p) == TOK_ALNUM || tkind(p) == TOK_DIGIT)
#define is_dlist(p) (tkind(p) == TOK_LBRAC)
#define is_sexpr(p) (is_alnum(p) || is_dlist(p) || tkind(p) == TOK_LPAREN)

#define CONSUME_EXPECT(p, k) do { \
    if (tkind(p) != k) \
        return expr_err(token_span(token(p)), "Expected '%s'", tk_to_str[k]); \
    (p)->pos++; \
} while (0)

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

// Get the span of the token.
static inline Span token_span(Token t)
{
    return (Span){t.pos, t.pos+t.value.len};
}

// Parse an alphanumeric number part, return error.
static Expr *parse_number_part_alnum(Parser *p, DigitArray *ds, Span *out)
{
    Token t = token(p);
    Span s = token_span(t);

    if (!is_alnum(p))
        return expr_err(s, "Expected alphanumerics");

    DigitResult res = digits_from_alnum(ds, t.value, p->base);
    s.from += res.pos;
    s.to = s.from+1;

    switch (res.kind)
    {
        case DIGIT_INVALID:
            return expr_err(s, "Not a digit");
        case DIGIT_OOB:
            return expr_err(s, "Digit out of bounds for base %lu", p->base);
        case DIGIT_BASE_TOO_LARGE:
            return expr_err(token_span(t), "Base too large for alphanumeric spelling");
        default:
            break;
    }
    p->pos++;
    out->to = token_span(t).to;
    return NULL;
}

// Parse a digit list number part, return error.
static Expr *parse_number_part_dlist(Parser *p, DigitArray *ds, Span *out)
{
    Token t = token(p);
    Span s = token_span(t);

    if (!is_dlist(p))
        return expr_err(s, "Expected digit list");

    CONSUME_EXPECT(p, TOK_LBRAC);
        for (;;)
        {
            t = token(p);
            if (tkind(p) != TOK_DIGIT)
                return expr_err(s, "Expected numeric value as digit");

            unsigned long val;
            if (!token_to_ul(t, &val) || val >= p->base)
                return expr_err(s, "Digit out of bounds");

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
static Expr *parse_number_part(Parser *p, DigitArray *ds, DigitFormat fmt, Span *out)
{
    switch (fmt)
    {
        case DIGIT_FMT_ALNUM:
            return parse_number_part_alnum(p, ds, out);

        case DIGIT_FMT_LIST:
            return parse_number_part_dlist(p, ds, out);

        default:
            return expr_err(tspan(p), "Expected number");
    }
}

// Parse a number literal in the form of I.N(R).
static Expr *parse_number(Parser *p, DigitFormat fmt)
{
    Expr *e = NULL;
    Span s = {0};

    Literal lit;
    literal_init(&lit);
    if ((e = parse_number_part(p, &lit.I, fmt, &s)))
        goto cleanup;


    if (tkind(p) != TOK_DOT)
        goto eval;
    p->pos++;

    if (tkind(p) != TOK_LPAREN)
    {
        if ((e = parse_number_part(p, &lit.N, fmt, &s)))
            goto cleanup;
    }

    if (tkind(p) == TOK_LPAREN)
    {
        p->pos++;
        if ((e = parse_number_part(p, &lit.R, fmt, &s)))
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
    return e;
}

// Parse a single expression in the base denoted by the tag.
static Expr *parse_base_tag(Parser *p)
{
    Token t = token(p);
    Span s = token_span(t);

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
    return expr_id(token_span(t), t.value);
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

// Parse a left denotation expression
static Expr *parse_led(Parser *p, int prec, Expr *left)
{
    Operator op = token_to_op(token(p));
    switch (op)
    {
        case OP_NIL:
            return expr_err(tspan(p), "Unknown operator");
        case OP_ASSIGN:
            if (left->kind != EXPR_IDENT)
                return expr_err(tspan(p), "Expected identifier");
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

    for (int pc = tprec(p); pc > prec; pc = tprec(p))
    {
        if (tkind(p) == TOK_EOF) break;
        e = parse_led(p, pc, e);
        if (is_error(e)) return e;
    }
    return e;
}

// Parse an expression.
Expr *parse(StringView src, unsigned long base)
{
    Span s = {0, 0};
    if (base == 0) base = BASE_DEFAULT;
    if (base == 1) return expr_err(s, "Base must be at least 2");

    TokenArray ta = {0};
    da_init(&ta);

    Expr *e = NULL;

    if (!tokenize(&ta, src))
    {
        e = expr_err(token_span(ta.data[0]), "Invalid token");
        goto cleanup;
    }

    Parser p = {
        .ta = &ta,
        .pos = 0,
        .base = base,
    };

    e = parse_expr(&p, PREC_PRIMARY);
    if (is_error(e)) goto cleanup;

    if (tkind(&p) != TOK_EOF)
    {
        expr_destroy(&e);
        e = expr_err(tspan(&p), "Trailing characters");
    }

cleanup:
    da_free(&ta);
    return e;
}
