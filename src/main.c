#include "vm.h"

#define CUT_IMPL
#include "cut.h"

// Start interactive REPL.
void repl_start(VM *vm)
{
    String in;
    str_reserve(&in, 1024);
    String out;
    str_reserve(&out, 1024);

    Value value = {0};

    for (;;)
    {
        printf("> ");

        str_readline(&in, stdin);
        StringView src = sv_trim(SV(in));

        if (src.len == 0) continue;

        if (sv_startswith(src, SV(":")))
        {
            if (sv_equal(src, ":q"))
                break;
            else
                printf("Unknown command.\n");

            continue;
        }

        vm_evaluate(vm, src, &value);

        RenderCtx ctx = {
            .src = src,
            .base = 10,
            .max_digits = 50,
            .num_form = NUMBER_DECIMAL,
        };
        vm_value_render(&value, &out, &ctx);
        printf(SV_FMT"\n", SV_ARG(SV(out)));

        str_reset(&in);
        str_reset(&out);
    }

    str_free(&in);
    str_free(&out);
}

// Run/evaluate a single expression and return the exit code.
int run_oneshot(VM *vm, FILE *fdout, StringView src)
{
    String s;
    str_reserve(&s, 1024);

    Value value = {0};

    bool ok = vm_evaluate(vm, src, &value);
    RenderCtx ctx = {
        .src = src,
        .base = 10,
        .max_digits = 50,
        .num_form = NUMBER_RATIONAL,
    };
    vm_value_render(&value, &s, &ctx);
    fprintf(fdout, SV_FMT"\n", SV_ARG(SV(s)));

    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    VM vm;
    vm_init(&vm);

    StringView args[argc];
    for (int i = 0; i < argc; i++)
        args[i] = SV(argv[i]);

    if (argc == 1)
    {
        repl_start(&vm);
    }
    else if (argc == 3 && sv_equal(args[1], "run"))
    {
        run_oneshot(&vm, stdout, args[2]);
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
