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
    TokenArray ta = {0};

    Parser p;
    parser_init(&p, &ta);

    Expr *e = NULL;

    mpq_t value;
    mpq_init(value);

    ReplMode mode = REPL_EVAL;

    for (;;)
    {
        parser_reset(&p);

        printf("> ");
        char src[1024];
        if (!fgets(src, sizeof(src), stdin))
            break;

        src[strcspn(src, "\n")] = '\0';

        if (strlen(src) == 0) continue;

        if (src[0] == ':')
        {
            if (strcmp(src, ":q") == 0)
                break;

            if (strcmp(src, ":eval") == 0)
                mode = REPL_EVAL;

            else if (strcmp(src, ":ast") == 0)
                mode = REPL_AST;

            else if (strcmp(src, ":token") == 0)
                mode = REPL_TOKEN;

            else
                printf("Unknown command.\n");

            continue;
        }

        if (!tokenize(&ta, src))
        {
            printf("Invalid expression.\n");
            continue;
        }

        if (mode != REPL_TOKEN)
        {
            e = parse_expr(&p, PREC_PRIMARY);
            if (is_error(e))
            {
                diagnostics_print(e, src);
                continue;
            }
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
                ta_print(&ta);
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
