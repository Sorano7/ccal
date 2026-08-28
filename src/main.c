#include "vm.h"

#define CUT_IMPL
#include "cut.h"

const char cli_help[] = "Commands:\n"
                        "     ccal help                  show this help\n"
                        "     ccal <opts>                start interactive REPL\n"
                        "     ccal run <opts> <expr>     evaluate and output the result\n"
                        "\n"
                        "Options:\n"
                        "    -d|--decimal                set the output form to decimal\n"
                        "    -r|--rational               set the output form to rational\n"
                        "    -o|--obase n                set the output base to n\n"
                        "    -i|--ibase n                set the default input base to n\n"
                        "    -t|--truncate n             set the max number of digits in decimal form\n"
;

const char repl_help[] = "Commands:\n"
                         "    :h, :help           show this help\n"
                         "    :q, :quit           exit the REPL\n"
                         "    :s, :set            set one of the options\n"
                         "\n"
                         "Options:\n"
                         "    dec|decimal         set the output form to decimal\n"
                         "    rat|rational        set the output form to rational\n"
                         "    ob|obase=n          set the output base to n\n"
                         "    ib|ibase=n          set the default input base to n\n"
                         "    tr|truncate=n       set the max number of digits in decimal form\n"
;

void repl_set_param_value(StringView s, unsigned long *v)
{
    if (s.len == 0)
    {
        printf("missing value\n");
        return;
    }
    int val = 0;
    if (!sv_to_int(s, &val))
    {
        printf("invalid value\n");
        return;
    }

    *v = val;
}

bool repl_handle_command(VM *vm, RenderCtx *ctx, StringView src)
{

    StringView cmd = sv_split(&src, ' ');

    if (sv_equal(cmd, "q") || sv_equal(cmd, "quit"))
    {
        printf("exit\n");
        return false;
    }
    else if (sv_equal(cmd, "h") || sv_equal(cmd, "help"))
    {
        printf(repl_help);
    }
    else if (sv_equal(cmd, "s") || sv_equal(cmd, "set"))
    {
        StringView param = sv_split(&src, '=');
        if (param.len == 0)
        {
            printf("missing option\n");
            return true;
        }

        if (sv_equal(param, "dec") || sv_equal(param, "decimal"))
        {
            ctx->num_form = NUMBER_DECIMAL;
            printf("output form: decimal\n");
        }
        else if (sv_equal(param, "rat") || sv_equal(param, "rational"))
        {
            ctx->num_form = NUMBER_RATIONAL;
            printf("output form: rational\n");
        }
        else if (sv_equal(param, "ob") || sv_equal(param, "obase"))
        {
            repl_set_param_value(src, &ctx->base);
            printf("output base: %lu\n", ctx->base);
        }
        else if (sv_equal(param, "ib") || sv_equal(param, "ibase"))
        {
            repl_set_param_value(src, &vm->base);
            printf("input base: %lu\n", vm->base);
        }
        else if (sv_equal(param, "tr") || sv_equal(param, "truncate"))
        {
            repl_set_param_value(src, &ctx->max_digits);
            printf("truncate at: %lu\n", ctx->max_digits);
        }
        else
        {
            printf("unknown option\n");
        }
    }
    else
    {
        printf("unknown command\n");
    }

    return true;
}

// Start interactive REPL.
void repl_start(VM *vm, RenderCtx *ctx)
{
    String in;
    str_reserve(&in, 1024);
    String out;
    str_reserve(&out, 1024);

    Value value = {0};

    for (;;)
    {
        printf("> ");

        str_reset(&in);
        str_readline(&in, stdin);
        StringView src = sv_trim(SV(in));

        if (src.len == 0) continue;

        if (sv_startswith(src, SV(":")))
        {
            sv_shift(&src, 1);
            if (!repl_handle_command(vm, ctx, src))
                break;
            continue;
        }

        vm_evaluate(vm, src, &value);

        ctx->src = src;
        vm_value_render(&value, &out, ctx);
        printf(SV_FMT"\n", SV_ARG(SV(out)));

        str_reset(&out);
    }

    str_free(&in);
    str_free(&out);
}

// Run/evaluate a single expression and return the exit code.
int run_oneshot(VM *vm, FILE *fdout, StringView src, RenderCtx *ctx)
{
    String s;
    str_reserve(&s, 1024);

    Value value = {0};

    bool ok = vm_evaluate(vm, src, &value);
    ctx->src =src;
    vm_value_render(&value, &s, ctx);
    fprintf(fdout, SV_FMT"\n", SV_ARG(SV(s)));

    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    VM vm;
    vm_init(&vm);

    RenderCtx ctx = {
        .base = 10,
        .max_digits = 50,
        .num_form = NUMBER_RATIONAL,
    };

    CutFlagParser fp;
    cut_fp_init(&fp);

    SVList args;
    da_init(&args);

    bool rational = false;
    bool decimal = false;

    cut_fp_add_command(&fp, SV("run"));
    cut_fp_add_command(&fp, SV("help"));

    cut_fp_add_flag(&fp, (int *)(&vm.base),        SV("ibase"), .short_name='i');
    cut_fp_add_flag(&fp, (int *)(&ctx.base),       SV("obase"), .short_name='o');
    cut_fp_add_flag(&fp, (int *)(&ctx.max_digits), SV("truncate"), .short_name='t');

    cut_fp_add_flag(&fp, &rational, SV("rational"), .short_name='r');
    cut_fp_add_flag(&fp, &decimal,  SV("decimal"),  .short_name='d');

    cut_fp_parse(&fp, argc, argv, &args);

    if (decimal && rational)
    {
        fprintf(stderr, "only one output form can be specified\n");
        return 1;
    }

    if (decimal)
        ctx.num_form = NUMBER_DECIMAL;

    StringView cmd = cut_fp_get_command(&fp, argc, argv);
    if (sv_equal(cmd, "help"))
    {
        printf(cli_help);
    }
    else if (sv_equal(cmd, "run"))
    {
        String sb;
        str_init(&sb);

        if (args.len == 0)
        {
            fprintf(stderr, "missing expression\n");
            return 1;
        }

        svlist_join(&args, &sb, SV(" "));

        return run_oneshot(&vm, stdout, SV(sb), &ctx);
    }
    else
    {
        repl_start(&vm, &ctx);
    }

    return 0;
}
