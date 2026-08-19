#include <stdio.h>
#include <string.h>

#include "vm.h"

// Start REPL.
void repl_start(void)
{
    VM vm;
    vm_init(&vm);

    mpq_t value;
    mpq_init(value);

    for (;;)
    {
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

            else
                printf("Unknown command.\n");

            continue;
        }

        VMResult res = vm_evaluate(&vm, src, value);

        if (res.ok)
            gmp_printf("%Qd\n", value);
        else
            vm_diagnostics(&res, src);

        vm_result_free(&res);
    }

    mpq_clear(value);
}

int main(int argc, char **argv)
{
    if (argc == 1)
    {
        repl_start();
    }
    else if (argc == 2 && strcmp(argv[1], "help") == 0)
    {
        printf("Usage:\n"
            "    ccal help     show this help\n"
            "    ccal          start REPL\n");
    }

    return 0;
}
