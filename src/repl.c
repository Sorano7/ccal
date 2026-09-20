#include "repl.h"
#include <readline/readline.h>
#include <readline/history.h>

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
        else if (sv_equal(src, "sci") || sv_equal(src, "scientific"))
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
        source_reset(ctx->src);
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

static const Scope *env = NULL;

// Generate a symbol completion from user-defined and builtin pools.
char *symbol_generator(const char *text, int state)
{
    static size_t idx;
    static SVList list = {0};

    if (!state)
    {
        idx = 0;

        if (!list.data)
        {
            da_init(&list);
        }
        else
        {
            da_reset(&list);
        }
        matching_symbol_list(env, SV(text), &list);
    }

    while (idx < list.len)
        return sv_alloc_cstr(list.data[idx++]);

    return NULL;
}

static const char *commands[] = {
    "help", "quit", "env", "clear", "set", NULL
};

static const char *set_options[] = {
    "obase", "ibase", "precision", "truncate", "format", "rational", NULL
};

static const char *fmt_options[] = {
    "auto", "fixed", "scientific", NULL
};

// Generate a match from a fixed, null-terminated array.
char *fixed_match_generator(const char **list, const char *text, int state)
{
    static size_t idx;
    const char *cand;

    if (!state) idx = 0;
    while ((cand = list[idx++]))
    {
        if (sv_startswith(SV(cand), SV(text)))
            return strdup(cand);
    }
    return NULL;
}

char *command_generator(const char *text, int state)
{
    return fixed_match_generator(commands, text, state);
}

char *set_option_generator(const char *text, int state)
{
    return fixed_match_generator(set_options, text, state);
}

char *fmt_option_generator(const char *text, int state)
{
    return fixed_match_generator(fmt_options, text, state);
}

// Completion function for REPL.
char **repl_completion(const char *text, int start, int end)
{
    (void)start, (void)end;

    rl_attempted_completion_over = 1;
    rl_completion_suppress_quote = 1;
    rl_completion_append_character = '\0';
    rl_completion_quote_character = '\0';

    StringView line_buf = SV(rl_line_buffer);
    if (sv_startswith(line_buf, SV(":set fmt "))
            || sv_startswith(line_buf, SV(":set format ")))
        return rl_completion_matches(text, fmt_option_generator);

    if (sv_startswith(line_buf, SV(":set ")))
        return rl_completion_matches(text, set_option_generator);

    if (sv_startswith(line_buf, SV(":")))
        return rl_completion_matches(text, command_generator);

    return rl_completion_matches(text, symbol_generator);
}

// Clear terminal.
int clear_screen(int count, int key)
{
    (void)count; (void)key;
    printf("\e[H\e[2J");
    fflush(stdout);
    rl_forced_update_display();
    return 0;
}

static bool read_logical_line(VM *vm, String *sb)
{
    const char *prompt = "ccal> ";

    for (;;)
    {
        char *line = readline(prompt);
        if (!line) return false;
        add_history(line);

        prompt = "..... ";

        StringView part = SV(line);
        if (part.len == 0) break;

        str_append(sb, part);
        free(line);

        if (sv_endswith(SV(sb), SV(";")))
        {
            continue;
        }
        if (sv_endswith(SV(sb), SV("\\")))
        {
            sb->len--;
            continue;
        }
        if (!vm_is_complete(vm, SV(sb)))
        {
            str_append(sb, "\n");
            continue;
        }
        break;
    }
    return true;
}

int tab_handler(int count, int key)
{
    (void)count, (void)key;

    StringView line = sv_trim(SV(rl_line_buffer));
    if (line.len == 0)
    {
        rl_insert_text("    ");
        return 0;
    }
    if (rl_last_func == tab_handler)
        return rl_complete_internal('?');

    return rl_complete_internal(TAB);
}

// Start interactive REPL.
void repl_start(VM *vm, RenderCtx *ctx)
{
    rl_bind_key('\014', clear_screen);
    rl_bind_key('\t', tab_handler);

    rl_attempted_completion_function = repl_completion;
    rl_completer_word_break_characters = " :'`";
    rl_completer_quote_characters = "";
    rl_basic_quote_characters = "`";

    Source src;
    source_init(&src);
    ctx->src = &src;
    env = vm->scope;

    String sb;
    str_reserve(&sb, 256);

    for (;;)
    {
        size_t offset = source_get_offset(&src);

        str_reset(&sb);
        if (!read_logical_line(vm, &sb))
            goto exit;

        if (sb.len == 0) continue;

        str_append(&sb, "\n");
        source_append_line(&src, SV(sb));

        StringView input = SV(sb);

        if (sv_startswith(input, SV(":")))
        {
            sv_shift(&input, 1);
            if (!repl_handle_command(vm, ctx, input))
                break;

            str_reset(&sb);
            continue;
        }

        Value *result = vm_run_next(vm, input, offset);

        str_reset(&sb);
        value_render(result, &sb, ctx);
        value_release(&result);

        printf(SV_FMT"\n", SV_ARG(SV(sb)));
    }

exit:
    str_free(&sb);
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

// Run a script file.
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

