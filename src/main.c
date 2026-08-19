#include <stdio.h>
#include <string.h>

#include "vm.h"

// Start REPL.
void repl_start(VM *vm, mpq_t value)
{
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

        VMResult res = vm_evaluate(vm, src, value);

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
    VM vm;
    vm_init(&vm);

    mpq_t value;
    mpq_init(value);

    if (argc == 1)
    {
        repl_start(&vm, value);
    }
    else if (argc == 3 && strcmp(argv[1], "run") == 0)
    {
        VMResult res = vm_evaluate(&vm, argv[2], value);
        if (res.ok)
        {
            gmp_printf("%Qd\n", value);
            vm_result_free(&res);
            return 0;
        }
        else
        {
            vm_diagnostics(&res, argv[2]);
            vm_result_free(&res);
            return 1;
        }
    }
    else
    {
        printf("Usage:\n"
            "    ccal help          show this help\n"
            "    ccal               start interactive REPL\n"
            "    ccal run <expr>    evaluate and print the result\n");
        return 1;
    }
    return 0;
}
