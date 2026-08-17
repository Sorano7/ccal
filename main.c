#include <gmp.h>
#include <stdbool.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>

#define AS_LIST(name, ...) name,
#define AS_STR(name) [name] = #name,
#define WITH_STR(name, s) [name] = s,

#define TOKENS(X) \
    X(TOK_SPACE) \
    X(TOK_INVALID) \
    X(TOK_NUMBER) \
    X(TOK_PLUS) \
    X(TOK_MINUS) \
    X(TOK_STAR) \
    X(TOK_SLASH) \
    X(TOK_LPAREN) \
    X(TOK_RPAREN) \
    X(TOK_EOF)

typedef enum
{
    TOKENS(AS_LIST)
} TokenKind;

static const char *token_kind_names[] = {TOKENS(AS_STR)};

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

typedef struct
{
    const char *data;
    size_t len;
    size_t start;
    TokenKind kind;
} Token;

typedef struct
{
    const char *src;
    Token *tokens;
    size_t len;
    size_t cap;
} Lexer;

bool lexer_init(Lexer *l)
{
    l->src = NULL;
    l->cap = 128;
    l->len = 0;
    l->tokens = malloc(sizeof(Token) * l->cap);
    if (!l->tokens) return false;
    return true;
}

void lexer_reset(Lexer *l)
{
    l->src = NULL;
    l->cap = 128;
    l->len = 0;
}

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

void lexer_print(Lexer *l)
{
    for (size_t i = 0; i < l->len; i++)
    {
        Token tok = l->tokens[i];
        printf("%02zu: %s(", i, token_kind_names[tok.kind]);
        if (tok.len > 0)
            printf("%.*s", (int)tok.len, tok.data);
        printf(")\n");
    }
}

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

#define OPERATORS(X) \
    X(OP_NONE, "") \
    X(OP_ADD, "+") \
    X(OP_SUB, "-") \
    X(OP_MUL, "*") \
    X(OP_DIV, "/") \
    X(OP_NEG, "-")

typedef enum
{
    OPERATORS(AS_LIST)
} Operator;

static char *op_symbols[] = {
    OPERATORS(WITH_STR)
};

Operator token_as_infix_op(Token t)
{
    switch (t.kind)
    {
        case TOK_PLUS:  return OP_ADD;
        case TOK_MINUS: return OP_SUB;
        case TOK_STAR:  return OP_MUL;
        case TOK_SLASH: return OP_DIV;
        default:        return OP_NONE;
    }
}

Operator token_as_prefix_op(Token t)
{
    switch (t.kind)
    {
        case TOK_MINUS: return OP_NEG;
        default:        return OP_NONE;
    }
}

typedef enum
{
    PREC_PRIMARY,
    PREC_SUM,
    PREC_PRODUCT,
    PREC_PREFIX,
} OpPrec;

OpPrec token_get_prec(Token t)
{
    switch (t.kind)
    {
        case TOK_PLUS:
        case TOK_MINUS:
            return PREC_SUM;

        case TOK_STAR:
        case TOK_SLASH:
            return PREC_PRODUCT;

        default:
            return PREC_PRIMARY;
    }
}

typedef enum
{
    EXPR_ERROR,
    EXPR_NUMBER,
    EXPR_INFIX,
    EXPR_PREFIX,
} ExprKind;

typedef struct Expr
{
    union
    {
        struct
        {
            mpq_t value;
        } number;

        struct
        {
            struct Expr *left;
            Operator op;
            struct Expr *right;

        } infix;

        struct
        {
            Operator op;
            struct Expr *expr;
        } prefix;

        struct
        {
            const char *msg;
        } error;
    } as;
    ExprKind kind;

    size_t start;
} Expr;

Expr *expr_new(ExprKind kind)
{
    Expr *e = malloc(sizeof(Expr));
    assert(e);
    e->kind = kind;
    return e;
}

void expr_free(Expr *e)
{
    if (!e) return;
    switch (e->kind)
    {
        case EXPR_NUMBER:
            mpq_clear(e->as.number.value);
            break;

        case EXPR_INFIX:
            expr_free(e->as.infix.left);
            e->as.infix.left = NULL;
            expr_free(e->as.infix.right);
            e->as.infix.right = NULL;
            break;

        case EXPR_PREFIX:
            expr_free(e->as.prefix.expr);
            e->as.prefix.expr = NULL;
            break;

        default:
            break;
    }
}

Expr *expr_error(const char *msg)
{
    Expr *e = expr_new(EXPR_ERROR);
    e->as.error.msg = msg;
    return e;
}

bool is_error(Expr *e)
{
    return e->kind == EXPR_ERROR;
}

Expr *expr_number(mpq_t value)
{
    Expr *e = expr_new(EXPR_NUMBER);
    mpq_init(e->as.number.value);

    mpq_set(e->as.number.value, value);
    return e;
}

Expr *expr_infix(Expr *l, Operator op, Expr *r)
{
    Expr *e = expr_new(EXPR_INFIX);
    e->as.infix.left = l;
    e->as.infix.op = op;
    e->as.infix.right = r;
    return e;
}

Expr *expr_prefix(Operator op, Expr *expr)
{
    Expr *e = expr_new(EXPR_PREFIX);
    e->as.prefix.op = op;
    e->as.prefix.expr = expr;
    return e;
}

typedef struct
{
    Lexer *lex;
    size_t pos;
} Parser;

void parser_init(Parser *p, Lexer *l)
{
    p->lex = l;
    p->pos = 0;
}

void parser_reset(Parser *p)
{
    p->pos = 0;
}

Token parser_get_token(const Parser *p)
{
    return p->lex->tokens[p->pos];
}

void parser_advance(Parser *p)
{
    if (p->pos >= p->lex->len)
    {
        p->pos = p->lex->len-1;
        return;
    }
    p->pos++;
}

bool parser_is_eof(Parser *p)
{
    return parser_get_token(p).kind == TOK_EOF;
}

bool parser_expect(const Parser *p, TokenKind tk)
{
    Token t = parser_get_token(p);
    return t.kind == tk;
}

int parser_get_prec(const Parser *p)
{
    return token_get_prec(parser_get_token(p));
}

Expr *parser_error(Parser *p, const char *msg)
{
    Expr *err = expr_error(msg);
    err->start = parser_get_token(p).start;
    return err;
}

void parser_diagnostics(Parser *p, Expr *err)
{
    if (err->kind != EXPR_ERROR) return;

    printf("%s\n", p->lex->src);
    for (size_t i = 0; i < err->start; i++)
        printf(" ");

    printf("^ %s\n", err->as.error.msg);
}

#define ENSURE_OK(e) do { \
    if (is_error(e)) return e; \
} while (0)

Expr *parse_expr(Parser *p, int prec);

Expr *parse_infix_expr(Parser *p, Expr *left, int prec)
{
    Operator op = token_as_infix_op(parser_get_token(p));
    if (op == OP_NONE)
        return parser_error(p, "Unknown operator");
    parser_advance(p);

    Expr *right = parse_expr(p, prec);
    ENSURE_OK(right);

    return expr_infix(left, op, right);
}

Expr *parse_number(Parser *p)
{
    Token tok = parser_get_token(p);

    mpq_t v;
    mpq_init(v);

    char buffer[tok.len+1];
    memcpy(buffer, tok.data, tok.len);
    buffer[tok.len] = '\0';

    if (mpq_set_str(v, buffer, 10) != 0)
        return parser_error(p, "Invalid number literal");

    Expr *e = expr_number(v);
    mpq_clear(v);

    parser_advance(p);
    return e;
}

Expr *parse_group(Parser *p)
{
    parser_advance(p);
    Expr *e = parse_expr(p, PREC_PRIMARY);
    ENSURE_OK(e);

    if (!parser_expect(p, TOK_RPAREN))
        return parser_error(p, "Expected '('");

    parser_advance(p);
    return e;
}

Expr *parse_prefix(Parser *p)
{
    parser_advance(p);
    Expr *e = parse_expr(p, PREC_PREFIX);
    ENSURE_OK(e);

    e = expr_prefix(OP_NEG, e);
    return e;
}

Expr *parse_expr(Parser *p, int prec)
{
    Expr *e = NULL;

    Token tok = parser_get_token(p);

    switch (tok.kind)
    {
        case TOK_EOF:    return parser_error(p, "Unexpected EOF");

        case TOK_NUMBER: e = parse_number(p); break;
        case TOK_LPAREN: e = parse_group(p);  break;
        case TOK_MINUS:  e = parse_prefix(p); break;

        default:         return parser_error(p, "Unexpected token");
    }

    int current_prec = parser_get_prec(p);

    while (current_prec != PREC_PRIMARY && current_prec > prec)
    {
        if (parser_is_eof(p)) break;
        e = parse_infix_expr(p, e, current_prec);
        ENSURE_OK(e);

        current_prec = parser_get_prec(p);
    }

    return e;
}

#define printf_indent(indent, fmt, ...) do { \
    for (int i = 0; i < indent; i++) \
        printf("  "); \
    printf(fmt __VA_OPT__(,) __VA_ARGS__); \
} while (0)

void ast_print(Expr *e, int indent)
{
    if (!e)
    {
        printf("(empty)");
        return;
    }

    printf("{\n");
    indent++;
    switch (e->kind)
    {
        case EXPR_NUMBER:
            printf_indent(indent, "type: number,\n");
            printf_indent(indent, "");
            gmp_printf("value: %Qd,\n", e->as.number.value);
            break;

        case EXPR_INFIX:
            printf_indent(indent, "type: infix,\n");
            printf_indent(indent, "left: ");
            ast_print(e->as.infix.left, indent);
            printf_indent(indent, "op: %s\n", op_symbols[e->as.infix.op]);
            printf_indent(indent, "right: ");
            ast_print(e->as.infix.right, indent);
            break;

        case EXPR_PREFIX:
            printf_indent(indent, "type: prefix,\n");
            printf_indent(indent, "op: %s\n", op_symbols[e->as.prefix.op]);
            printf_indent(indent, "expr: ");
            ast_print(e->as.prefix.expr, indent);
            break;

        case EXPR_ERROR:
            printf_indent(indent, "type: error,\n");
            printf_indent(indent, "msg: %s\n", e->as.error.msg);
            break;
    }
    indent--;
    printf_indent(indent, "},\n");
}

typedef struct
{
    bool ok;
    const char *msg;
} EvalResult;

#define eval_error(msg) (EvalResult){false, msg}
#define eval_ok() (EvalResult){true, NULL}

EvalResult evaluate(Expr *e, mpq_t out)
{
    EvalResult res = {0};
    if (!e) return eval_error("Empty expression");

    switch (e->kind)
    {
        case EXPR_ERROR:
            return eval_error("Invalid expression");

        case EXPR_NUMBER:
            mpq_set(out, e->as.number.value);
            break;

        case EXPR_INFIX:
            mpq_t l, r;
            mpq_inits(l, r, NULL);
            res = evaluate(e->as.infix.left, l);
            if (!res.ok)
            {
                mpq_clears(l, r, NULL);
                return res;
            }

            res = evaluate(e->as.infix.right, r);
            if (!res.ok)
            {
                mpq_clears(l, r, NULL);
                return res;
            }

            switch (e->as.infix.op)
            {
                case OP_ADD: mpq_add(out, l, r); break;
                case OP_SUB: mpq_sub(out, l, r); break;
                case OP_MUL: mpq_mul(out, l, r); break;

                case OP_DIV:
                    if (mpq_cmp_si(r, 0, 1) == 0)
                        return eval_error("Division by zero");
                    mpq_div(out, l, r);
                    break;

                default:
                     mpq_clears(l, r, NULL);
                     return eval_error("Unknown operator");
            }
            mpq_clears(l, r, NULL);
            break;

        case EXPR_PREFIX:
            res = evaluate(e->as.prefix.expr, out);
            if (!res.ok)
                return res;

            mpq_neg(out, out);
            break;
    }

    return eval_ok();
}

void repl_start(void)
{
    Lexer l;
    lexer_init(&l);

    Parser p;
    parser_init(&p, &l);

    Expr *e = NULL;

    mpq_t value;
    mpq_init(value);

    for (;;)
    {
        printf("> ");
        char buffer[1024];
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = '\0';

        if (!tokenize(&l, buffer))
        {
            printf("Invalid expression\n");
            continue;
        }

        e = parse_expr(&p, PREC_PRIMARY);
        if (is_error(e))
        {
            parser_diagnostics(&p, e);
            continue;
        }

        EvalResult res = evaluate(e, value);
        if (res.ok)
            gmp_printf("%Qd\n", value);
        else
            printf("Error: %s\n", res.msg);

        lexer_reset(&l);
        parser_reset(&p);
    }

    mpq_clear(value);
    expr_free(e);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "repl") == 0)
        repl_start();

    return 0;
}
