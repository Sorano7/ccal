#ifndef _WIN32
    #include <unistd.h>
#endif

#include "vm.h"
#include "repl.h"

#define CUT_IMPL
#include "cut.h"

int main(int argc, char **argv)
{
    VM vm;
    vm_init(&vm);

    RenderCtx ctx;
    render_ctx_default(&ctx);
    ctx.use_color = isatty(fileno(stdout));

    StringView fmt = SV("auto");

    CutFlagParser fp;
    cut_fp_init(&fp);

    SVList args;
    da_init(&args);

    cut_fp_add_command(&fp, SV("run"));
    cut_fp_add_command(&fp, SV("eval"));
    cut_fp_add_command(&fp, SV("help"));

    cut_fp_add_flag(&fp, (int *)(&ctx.base),       SV("obase"),     .short_name='o');
    cut_fp_add_flag(&fp, (int *)(&vm.base),        SV("ibase"),     .short_name='i');
    cut_fp_add_flag(&fp, (int *)(&ctx.prec),       SV("precision"), .short_name='p');
    cut_fp_add_flag(&fp, (int *)(&ctx.max_digits), SV("truncate"),  .short_name='t');
    cut_fp_add_flag(&fp, &fmt,                     SV("format"),    .short_name='f');
    cut_fp_add_flag(&fp, &ctx.show_rational,       SV("rational"),  .short_name='r');

    cut_fp_parse(&fp, argc, argv, &args);

    if      (sv_equal(fmt, "auto"))  ctx.fmt = FMT_AUTO;
    else if (sv_equal(fmt, "fixed")) ctx.fmt = FMT_FIXED;
    else if (sv_equal(fmt, "sci"))   ctx.fmt = FMT_SCI;
    else
    {
        fprintf(stderr, "Unknown format.\n"FORMAT_LIST);
        return 1;
    }

    bool ok = true;

    StringView cmd = cut_fp_get_command(&fp, argc, argv);
    if (sv_equal(cmd, "help"))
    {
        printf("%s", cli_help);
    }
    else if (sv_equal(cmd, "run"))
    {
        if (args.len == 0)
        {
            fprintf(stderr, "No input file.\n");
            return 1;
        }
        if (args.len > 1)
        {
            fprintf(stderr, "Too many arguments.\n");
            return 1;
        }

        ok = run_script(&vm, args.data[0], &ctx);
    }
    else if (sv_equal(cmd, "eval"))
    {
        String sb;
        str_init(&sb);

        if (args.len == 0)
        {
            fprintf(stderr, "Empty expression.\n");
            return 1;
        }

        svlist_join(&args, &sb, SV(" "));
        ok = run_eval(&vm, stdout, SV(sb), &ctx);
        str_free(&sb);
    }
    else
    {
        repl_start(&vm, &ctx);
    }

    cut_fp_free(&fp);
    da_free(&args);
    vm_free(&vm);
    return ok ? 0 : 1;
}
