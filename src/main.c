#include "vm.h"

#define CUT_IMPL
#include "cut.h"

// Start REPL.
void repl_start(VM *vm, Value *value)
{
    for (;;)
    {
        printf("> ");
        String s;
        str_reserve(&s, 1024);

        str_readline(&s, stdin);
        StringView src = sv_trim(SV(s));

        if (src.len == 0) continue;

        if (sv_startswith(src, SV(":")))
        {
            if (sv_equal(src, ":q"))
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

    StringView args[argc];
    for (int i = 0; i < argc; i++)
        args[i] = SV(argv[i]);

    if (argc == 1)
    {
        repl_start(&vm, &value);
    }
    else if (argc == 3 && sv_equal(args[1], "run") == 0)
    {
        bool ok = vm_evaluate(&vm, args[2], &value);
        vm_value_print(&value, args[2]);
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
