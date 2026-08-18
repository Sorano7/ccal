#include <stdio.h>
#include <string.h>

#include "parser.h"
#include "lexer.h"
#include "expr.h"
#include "eval.h"

// Output mode of the REPL.
typedef enum
{
    REPL_EVAL,
    REPL_AST,
    REPL_TOKEN,
} ReplMode;

// Start REPL.
void repl_start(void)
{
    Lexer l;
    lexer_init(&l);

    Parser p;
    parser_init(&p, &l);

    Expr *e = NULL;

    mpq_t value;
    mpq_init(value);

    ReplMode mode = REPL_EVAL;

    for (;;)
    {
        lexer_reset(&l);
        parser_reset(&p);

        printf("> ");
        char buffer[1024];
        if (!fgets(buffer, sizeof(buffer), stdin))
            break;

        buffer[strcspn(buffer, "\n")] = '\0';

        if (strlen(buffer) == 0) break;

        if (buffer[0] == ':')
        {
            if (strcmp(buffer, ":q") == 0)
                break;

            if (strcmp(buffer, ":eval") == 0)
                mode = REPL_EVAL;

            else if (strcmp(buffer, ":ast") == 0)
                mode = REPL_AST;

            else if (strcmp(buffer, ":token") == 0)
                mode = REPL_TOKEN;

            else
                printf("Unknown command.\n");

            continue;
        }

        if (!tokenize(&l, buffer))
        {
            printf("Invalid expression.\n");
            continue;
        }

        e = parse_expr(&p, PREC_PRIMARY);
        if (is_error(e))
        {
            parser_diagnostics(&p, e);
            continue;
        }

        switch (mode)
        {
            case REPL_EVAL:
                EvalResult res = evaluate(e, value);
                if (res.ok)
                    gmp_printf("%Qd\n", value);
                else
                    printf("Error: %s.\n", res.msg);
                break;

            case REPL_AST:
                ast_print(e, 0);
                break;

            case REPL_TOKEN:
                lexer_print(&l);
                break;
        }
    }

    mpq_clear(value);
    expr_free(e);
}

int main(int argc, char **argv)
{
    if (argc == 2 && strcmp(argv[1], "repl") == 0)
    {
        repl_start();
    }
    else
    {
        printf("Usage:\n");
        printf("    ccal help     show this help\n");
        printf("    ccal repl     start REPL\n");
    }

    return 0;
}
