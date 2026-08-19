#include <stdio.h>
#include <string.h>

#include "vm.h"

// Start REPL.
void repl_start(VM *vm, Value *value)
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

        vm_evaluate(vm, src, value);
        vm_value_print(value, src);
    }
}

int main(int argc, char **argv)
{
    VM vm;
    vm_init(&vm);

    Value value = {0};

    if (argc == 1)
    {
        repl_start(&vm, &value);
    }
    else if (argc == 3 && strcmp(argv[1], "run") == 0)
    {
        bool ok = vm_evaluate(&vm, argv[2], &value);
        vm_value_print(&value, argv[2]);
        return ok ? 0 : 1;
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
