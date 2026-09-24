#ifndef _WIN32
    #include <unistd.h>
#endif

#include "repl.h"
#include "ccal/ccal.h"
#include "cut.h"

#include <readline/readline.h>
#include <readline/history.h>

const char cli_help[] =  "Commands:\n"
                         "     ccal help                  show this help\n"
                         "     ccal <opts>                start interactive REPL\n"
                         "     ccal run  <opts> <path>    run a script and output the result\n"
                         "     ccal eval <opts> <expr>    evaluate and output the result\n"
                         "\n"
                         "Options:\n"
                         "    -o | --obase    <n>         "DESC_OBASE    "\n"
                         "    -i | --ibase    <n>         "DESC_IBASE    "\n"
                         "    -p | --prec     <n>         "DESC_PRECISION"\n"
                         "    -t | --truncate <n>         "DESC_TRUNCATE "\n"
                         "    -f | --format   <fmt>       "DESC_FORMAT   "\n"
                         "    -r | --rational             "DESC_RATIONAL "\n"
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
    if (ccal_get_show_color(vm)) printf(AFMT_RESET"%s", (c)); \
    printf(s __VA_OPT__(,) __VA_ARGS__); \
    if (ccal_get_show_color(vm)) printf(AFMT_RESET); \
} while (0)

#define appendc(sb, c, s, ...) do { \
    if (ccal_get_show_color(vm)) str_appendf((sb), AFMT_RESET"%s", (c)); \
    str_appendf((sb), (s) __VA_OPT__(,) __VA_ARGS__); \
    if (ccal_get_show_color(vm)) str_append((sb), AFMT_RESET); \
} while (0)

#define is_either(str, s, l) (sv_equal((str), (s)) || sv_equal((str), (l)))

#define SET_NUM(fn) do { \
    if (opt.len == 0) { \
        printc(ACOLOR_RED, "Missing value.\n"); \
        break; \
    } \
    int val = 0; \
    if (!sv_to_int(opt, &val)) { \
        printc(ACOLOR_RED, "Invalid value.\n"); \
        break; \
    } \
    (fn)(vm, val); \
} while (0)

static void handle_set_command(CCalVM *vm, StringView input)
{
    bool has_eq = sv_find(input, '=') != SIZE_MAX;
    StringView param = sv_split(&input, has_eq ? '=' : ' ');
    param = sv_trim(param);
    StringView opt = sv_trim(input);

    if (param.len == 0)
    {
        printc(ACOLOR_RED, "Missin option.\n");
        printc(ACOLOR_CYAN, OPTIONS_REPL);
    }
    else if (is_either(param, "ob", "obase"))
    {
        SET_NUM(ccal_set_obase);
        printc(AFMT_DIM, "Output base: ");
        printc(ACOLOR_CYAN, "%lu\n", ccal_get_obase(vm));
    }
    else if (is_either(param, "ib", "ibase"))
    {
        SET_NUM(ccal_set_ibase);
        printc(AFMT_DIM, "Input base: ");
        printc(ACOLOR_CYAN, "%lu\n", ccal_get_ibase(vm));
    }
    else if (is_either(param, "tr", "truncate"))
    {
        SET_NUM(ccal_set_max_digits);
        printc(AFMT_DIM, "Truncate: ");
        printc(ACOLOR_CYAN, "%lu\n", ccal_get_max_digits(vm));
    }
    else if (is_either(param, "pr", "precision"))
    {
        SET_NUM(ccal_set_prec);
        printc(AFMT_DIM, "Precision: ");
        printc(ACOLOR_CYAN, "%lu\n", ccal_get_prec(vm));
    }
    else if (is_either(param, "rat", "rational"))
    {
        ccal_set_show_rational(vm, !ccal_get_show_rational(vm));
        printc(AFMT_DIM, "Show rational: ");
        printc(ACOLOR_CYAN, "%s\n",  ccal_get_show_rational(vm) ? "on" : "off");
    }
    else if (is_either(param, "fmt", "format"))
    {
        if (sv_equal(opt, "auto"))
        {
            ccal_set_format(vm, CCAL_FMT_AUTO);
            printc(AFMT_DIM, "Display format: ");
            printc(ACOLOR_CYAN, "auto\n");
        }
        else if (sv_equal(opt, "fixed"))
        {
            ccal_set_format(vm, CCAL_FMT_FIXED_POINT);
            printc(AFMT_DIM, "Display format: ");
            printc(ACOLOR_CYAN, "fixed\n");
        }
        else if (is_either(opt, "sci", "scientific"))
        {
            ccal_set_format(vm, CCAL_FMT_SCIENTIFIC);
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

static bool handle_command(CCalVM *vm, StringView input)
{
    if (ccal_get_show_color(vm)) printf(ACOLOR_CYAN);

    input = sv_trim(input);
    StringView cmd = sv_split(&input, ' ');

    bool exit = false;

    if (is_either(cmd, "q", "quit"))
    {
        exit = true;
    }
    else if (is_either(cmd, "h", "help"))
    {
        printf(repl_help);
    }
    else if (is_either(cmd, "e", "env"))
    {
        char *s = ccal_render_env(vm);
        printf("%s", s);
        free(s);
    }
    else if (is_either(cmd, "c", "clear"))
    {
        ccal_reset(vm);
        printf("Cleared.\n");
    }
    else if (is_either(cmd, "s", "set"))
    {
        handle_set_command(vm, input);
    }
    else
    {
        printc(ACOLOR_RED, "Unknown command.\n");
        printc(ACOLOR_CYAN, repl_help);
    }

    return exit;
}

static const CCalVM *vm_ref;

// Generate a symbol completion from user-defined and builtin pools.
char *symbol_generator(const char *text, int state)
{
    static size_t idx, len;
    static char **symbols = NULL;

    if (!state)
    {
        idx = 0;
        if (symbols) ccal_free_symbols(symbols, len);
        symbols = ccal_symbols(vm_ref, &len);
        if (!symbols) return NULL;
    }

    while (idx < len)
    {
        char *symbol = symbols[idx++];
        if (sv_startswith(SV(symbol), SV(text)))
            return strdup(symbol);
    }
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

static bool read_logical_line(CCalVM *vm, String *sb)
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
        if (!ccal_expr_complete(vm, sb->data))
        {
            str_append(sb, "\n");
            continue;
        }
        break;
    }
    str_append(sb, "\n");
    return true;
}

void repl_start(const CCalCtx *ctx)
{
    rl_bind_key('\014', clear_screen);
    rl_bind_key('\t', tab_handler);
    rl_attempted_completion_function = repl_completion;
    rl_completer_word_break_characters = " :'`";
    rl_completer_quote_characters = "";
    rl_basic_quote_characters = "`";

    CCalVM *vm = ccal_create();
    ccal_set_ctx(vm, ctx);

    vm_ref = vm;

    String sb;
    str_reserve(&sb, 256);

    for (;;)
    {
        str_reset(&sb);
        if (!read_logical_line(vm, &sb))
            break;

        StringView input = SV(sb);
        if (sv_trim(input).len == 0)
            continue;

        if (sv_startswith(input, SV(":")))
        {
            sv_shift(&input, 1);
            if (handle_command(vm, input))
                break;

            str_reset(&sb);
            continue;
        }

        CCalResult result = ccal_eval(vm, input.data);

        char *display = ccal_render(vm, result.value);
        printf("%s\n", display);

        free(display);
        ccal_release(result.value);
    }

    printc(ACOLOR_CYAN, "Exit.\n");

    str_free(&sb);
    ccal_free(vm);
}

bool run_eval(FILE *fdout, StringView input, const CCalCtx *ctx)
{
    CCalVM *vm = ccal_create();
    ccal_set_ctx(vm, ctx);

    char *src = sv_alloc_cstr(input);
    CCalResult res = ccal_eval(vm, src);
    free(src);

    char *display = ccal_render(vm, res.value);
    fprintf(fdout, "%s\n", display);

    free(display);
    ccal_release(res.value);

    return res.ok;
}

bool run_script(StringView path, const CCalCtx *ctx)
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

    bool ok = run_eval(stdout, SV(buf), ctx);
    str_free(&buf);
    return ok;
}

