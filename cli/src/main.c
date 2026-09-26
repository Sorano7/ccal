#include "repl.h"

#define CUT_IMPL
#include "cut.h"

int main(int argc, char **argv)
{
    CCalCtx ctx = CCAL_CTX_DEFAULT;
    ctx.show_color = isatty(fileno(stdout));

    StringView fmt = SV("auto");

    CutFlagParser fp;
    cut_fp_init(&fp);

    SVList args;
    da_init(&args);

    cut_fp_add_commands(&fp, "help", "run", "eval");

    cut_fp_add_flag(&fp, (int *)(&ctx.obase),      SV("obase"),     .short_name='o');
    cut_fp_add_flag(&fp, (int *)(&ctx.ibase),      SV("ibase"),     .short_name='i');
    cut_fp_add_flag(&fp, (int *)(&ctx.precision),  SV("precision"), .short_name='p');
    cut_fp_add_flag(&fp, (int *)(&ctx.max_digits), SV("truncate"),  .short_name='t');
    cut_fp_add_flag(&fp, &fmt,                     SV("format"),    .short_name='f');
    cut_fp_add_flag(&fp, &ctx.show_rational,       SV("rational"),  .short_name='r');

    CutFPResult res = cut_fp_parse(&fp, argc, argv, &args);
    if (res.status != CUT_FP_OK)
    {
        fprintf(stderr, "Error parsing flags: "SV_FMT"\n", SV_ARG(SV(res.msg)));
        return 1;
    }
    StringView cmd = fp.subcmd;
    cut_fp_free(&fp);

    if      (sv_equal(fmt, "auto"))  ctx.render_fmt = CCAL_FMT_AUTO;
    else if (sv_equal(fmt, "fixed")) ctx.render_fmt = CCAL_FMT_FIXED_POINT;
    else if (sv_equal(fmt, "sci"))   ctx.render_fmt = CCAL_FMT_SCIENTIFIC;
    else
    {
        fprintf(stderr, "Unknown format.\n"FORMAT_LIST);
        return 1;
    }

    bool ok = true;

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

        ok = run_script(args.data[0], &ctx);
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
        ok = run_eval(stdout, SV(sb), &ctx);
        str_free(&sb);
    }
    else
    {
        repl_start(&ctx);
    }

    da_free(&args);
    return ok ? 0 : 1;
}
