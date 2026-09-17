#include "vm.h"
#include <readline/readline.h>
#include <readline/history.h>

#ifndef _WIN32
    #include <unistd.h>
#endif

#define CUT_IMPL
#include "cut.h"

#define FORMAT    "set the display format for numeric values"
#define AUTO      "fixed-point by default, scientific for large exponent"
#define FIXED     "force fixed-point notation"
#define SCI       "force scientific notation"

#define RATIONAL  "display rational form alongside output for exact values"
#define OBASE     "set the output base"
#define IBASE     "set the default input base"
#define PRECISION "set the precision for real value"
#define TRUNCATE  "set the max decimal places to truncate at"

#define FORMAT_LIST \
                    "Display Formats:\n" \
                    "    auto                        "AUTO     "\n" \
                    "    fixed                       "FIXED    "\n" \
                    "    sci | scientific            "SCI      "\n"

#define OPTIONS_REPL \
                    "Options:\n" \
                    "    ob  | obase     <n>         "OBASE    "\n" \
                    "    ib  | ibase     <n>         "IBASE    "\n" \
                    "    pr  | precision <n>         "PRECISION"\n" \
                    "    tr  | truncate  <n>         "TRUNCATE "\n" \
                    "    fmt | format    <fmt>       "FORMAT   "\n" \
                    "    rat | rational              "RATIONAL "\n"

const char cli_help[] =  "Commands:\n"
                         "     ccal help                  show this help\n"
                         "     ccal <opts>                start interactive REPL\n"
                         "     ccal run  <opts> <path>    run a script and output the result\n"
                         "     ccal eval <opts> <expr>    evaluate and output the result\n"
                         "\n"
                         "Options:\n"
                         "    -o | --obase    <n>         "OBASE    "\n"
                         "    -i | --ibase    <n>         "IBASE    "\n"
                         "    -p | --prec     <n>         "PRECISION"\n"
                         "    -t | --truncate <n>         "TRUNCATE "\n"
                         "    -f | --format   <fmt>       "FORMAT   "\n"
                         "    -r | --rational             "RATIONAL "\n"
                         "\n"
                         FORMAT_LIST
;

const char repl_help[] = "Commands:\n"
                         "    :h | :help                  show this help\n"
                         "    :q | :quit                  exit the REPL\n"
                         "    :e | :env                   show current environment\n"
                         "    :c | :clear                 clear current environment\n"
                         "    :s | :set                   set options for the REPL\n"
                         "\n"
                         OPTIONS_REPL
                         "\n"
                         FORMAT_LIST
;

int clear_screen(int count, int key)
{
    (void)count; (void)key;
    printf("\e[H\e[2J");
    fflush(stdout);
    rl_forced_update_display();
    return 0;
}

#define printc(c, s, ...) do { \
    if (ctx->use_color) printf(AFMT_RESET"%s", (c)); \
    printf(s __VA_OPT__(,) __VA_ARGS__); \
    if (ctx->use_color) printf(AFMT_RESET); \
} while (0)

#define appendc(sb, c, s, ...) do { \
    if (ctx->use_color) str_appendf((sb), AFMT_RESET"%s", (c)); \
    str_appendf((sb), (s) __VA_OPT__(,) __VA_ARGS__); \
    if (ctx->use_color) str_append((sb), AFMT_RESET); \
} while (0)

// Setting the parameter with the set command.
static bool repl_set_numeric(StringView s, unsigned long *v, RenderCtx *ctx)
{
    s = sv_trim(s);
    if (s.len == 0)
    {
        printc(ACOLOR_RED, "Missing value.\n");
        return false;
    }
    int val = 0;
    if (!sv_to_int(s, &val))
    {
        printc(ACOLOR_RED, "Invalid value.\n");
        return false;
    }
    *v = val;
    return true;
}

// Handle the set command.
static void repl_handle_set_command(VM *vm, RenderCtx *ctx, StringView src)
{
    StringView param;
    if (sv_find(src, '=') != SIZE_MAX)
        param = sv_split(&src, '=');
    else
        param = sv_split(&src, ' ');

    param = sv_trim(param);

    if (param.len == 0)
    {
        printc(ACOLOR_RED, "Missin option.\n");
        printc(ACOLOR_CYAN, OPTIONS_REPL);
    }
    else if (sv_equal(param, "ob") || sv_equal(param, "obase"))
    {
        if (!repl_set_numeric(src, &ctx->base, ctx)) return;
        printc(AFMT_DIM, "Output base: ");
        printc(ACOLOR_CYAN, "%lu\n", ctx->base);
    }
    else if (sv_equal(param, "ib") || sv_equal(param, "ibase"))
    {
        if (!repl_set_numeric(src, &vm->base, ctx)) return;
        printc(AFMT_DIM, "Input base: ");
        printc(ACOLOR_CYAN, "%lu\n", vm->base);
    }
    else if (sv_equal(param, "pr") || sv_equal(param, "precision"))
    {
        if (!repl_set_numeric(src, (unsigned long *)&ctx->prec, ctx)) return;
        printc(AFMT_DIM, "Precision: ");
        printc(ACOLOR_CYAN, "%lu\n", ctx->prec);
    }
    else if (sv_equal(param, "tr") || sv_equal(param, "truncate"))
    {
        if (!repl_set_numeric(src, &ctx->max_digits, ctx)) return;
        printc(AFMT_DIM, "Truncate: ");
        printc(ACOLOR_CYAN, "%lu\n", ctx->max_digits);
    }
    else if (sv_equal(param, "rat") || sv_equal(param, "rational"))
    {
        ctx->show_rational = !ctx->show_rational;
        printc(AFMT_DIM, "Show rational: ");
        printc(ACOLOR_CYAN, "%s\n", ctx->show_rational ? "on" : "off");
    }
    else if (sv_equal(param, "fmt") || sv_equal(param, "format"))
    {
        if (sv_equal(src, "auto"))
        {
            ctx->fmt = FMT_AUTO;
            printc(AFMT_DIM, "Display format: ");
            printc(ACOLOR_CYAN, "auto\n");
        }
        else if (sv_equal(src, "fixed"))
        {
            ctx->fmt = FMT_FIXED;
            printc(AFMT_DIM, "Display format: ");
            printc(ACOLOR_CYAN, "fixed\n");
        }
        else if (sv_equal(src, "sci"))
        {
            ctx->fmt = FMT_SCI;
            printc(AFMT_DIM, "Display format: ");
            printc(ACOLOR_CYAN, "scientific\n");
        }
        else 
        {
            printc(ACOLOR_RED, "Unknown format.\n");
            printc(ACOLOR_CYAN, FORMAT_LIST);
        }
    }
    else
    {
        printc(ACOLOR_RED, "Unknown option.\n");
        printc(ACOLOR_CYAN, OPTIONS_REPL);
    }
}

// Handle REPL commands.
bool repl_handle_command(VM *vm, RenderCtx *ctx, StringView src)
{
    if (ctx->use_color) printf(ACOLOR_CYAN);

    src = sv_trim(src);
    StringView cmd = sv_split(&src, ' ');

    bool should_continue = true;

    if (sv_equal(cmd, "q") || sv_equal(cmd, "quit"))
    {
        should_continue = false;
    }
    else if (sv_equal(cmd, "h") || sv_equal(cmd, "help"))
    {
        printf(repl_help);
    }
    else if (sv_equal(cmd, "e") || sv_equal(cmd, "env"))
    {
        String sb;
        str_init(&sb);
        vm_env_render(vm, &sb, ctx);
        printf(SV_FMT, SV_ARG(SV(sb)));
        str_free(&sb);
    }
    else if (sv_equal(cmd, "c") || sv_equal(cmd, "clear"))
    {
        vm_reset(vm);
        printf("Cleared.\n");
    }
    else if (sv_equal(cmd, "s") || sv_equal(cmd, "set"))
    {
        repl_handle_set_command(vm, ctx, src);
    }
    else
    {
        printc(ACOLOR_RED, "Unknown command.\n");
        printc(ACOLOR_CYAN, repl_help);
    }

    if (ctx->use_color) printf(AFMT_RESET);
    return should_continue;
}

// Start interactive REPL.
void repl_start(VM *vm, RenderCtx *ctx)
{
    rl_bind_key('\014', clear_screen);

    Source src;
    source_init(&src);
    ctx->src = &src;

    String in, out;
    str_reserve(&in, 256);
    str_reserve(&out, 256);

    char *line;

    for (;;)
    {
        const char *prompt = "ccal> ";

        for (;;)
        {
            line = readline(prompt);
            if (!line) goto exit;

            prompt = "..... ";

            StringView part = sv_trim(SV(line));
            if (part.len == 0) break;

            str_append(&in, part);
            free(line);

            if (sv_endswith(SV(in), SV(";")))
                continue;

            if (sv_endswith(SV(in), SV("\\")))
            {
                in.len--;
                continue;
            }

            break;
        }

        if (in.len == 0) continue;

        add_history(in.data);
        str_append(&in, "\n");

        StringView input = SV(in);
        if (sv_startswith(input, SV(":")))
        {
            sv_shift(&input, 1);
            if (!repl_handle_command(vm, ctx, input))
                break;

            str_reset(&in);
            continue;
        }

        size_t offset = source_get_offset(&src);
        Value *result = vm_run_next(vm, input, offset);
        source_append_line(&src, input);

        value_render(result, &out, ctx);
        value_release(&result);

        printf(SV_FMT"\n", SV_ARG(SV(out)));

        str_reset(&in);
        str_reset(&out);
    }

exit:
    str_free(&in);
    str_free(&out);
    source_free(&src);

    printc(ACOLOR_CYAN, "Exit.\n");
}

// Run/evaluate a single expression.
bool run_eval(VM *vm, FILE *fdout, StringView input, RenderCtx *ctx)
{
    String out;
    str_init(&out);

    Source src;
    source_init(&src);
    ctx->src = &src;

    Value *result = vm_run(vm, input, &src);
    value_render(result, &out, ctx);
    fprintf(fdout, SV_FMT"\n", SV_ARG(SV(out)));

    bool ok = !value_is_err(result);
    value_release(&result);

    str_free(&out);
    source_free(&src);
    return ok;
}

bool run_script(VM *vm, StringView path, RenderCtx *ctx)
{
    String buf;
    str_init(&buf);

    SV_TO_CSTR(path, path_buf);

    FILE *f = fopen(path_buf, "r");
    if (!f)
    {
        fprintf(stderr, "Error opening "SV_FMT, SV_ARG(path));
        return false;
    }

    if (!str_readfile(&buf, f))
    {
        fprintf(stderr, "Error reading "SV_FMT, SV_ARG(path));
        return false;
    }
    fclose(f);

    bool ok = run_eval(vm, stdout, SV(buf), ctx);
    str_free(&buf);
    return ok;
}

int main(int argc, char **argv)
{
    VM vm;
    vm_init(&vm);

    RenderCtx ctx = {
        .base          = 10,
        .prec          = 50,
        .max_digits    = 10,
        .fmt           = FMT_AUTO,
        .show_rational = false,
        .use_color     = isatty(fileno(stdout)),
    };
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
        printf(cli_help);
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
