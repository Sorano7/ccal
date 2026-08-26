#ifndef CUT_H
#define CUT_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

/************************************************
 * Utils
 ************************************************/

// Default capacity for any dynamic array.
#define DA_DEFAULT_CAP 64

// Initializes and reserves capacity for a dynamic array.
#define da_reserve(da, c) do { \
    (da)->len = 0; \
    (da)->cap = (c) == 0 ? DA_DEFAULT_CAP : (c); \
    (da)->data = malloc((da)->cap * sizeof(*(da)->data)); \
} while (0)

// Intializes a dynamic array.
#define da_init(da) da_reserve(da, DA_DEFAULT_CAP);

// Free a dynamic array.
#define da_free(da) do { \
    (da)->len = 0; \
    (da)->cap = 0; \
    if ((da)->data) free((da)->data); \
    (da)->data = NULL; \
} while (0)

// Reset a dynamic array.
#define da_reset(da) do { \
    (da)->len = 0; \
} while (0)

#define da_for(da, i) for (size_t i = 0; i < (da)->len; i++)

// Map a function accepting and returning type to each item in the dynamic array.
#define da_map(da, type, f) do { \
    da_for((da), i) { \
        type item = (da)->data[i]; \
        (da)->data[i] = (f)(item); \
    } \
} while (0)

// Map a function accepting pointer to type to each item in the dynamic array.
#define da_map_mut(da, type, f) do { \
    da_for((da), i) { \
        (f)((type *)(&(da)->data[i])); \
    } \
} while (0)

// Append an item to the dynamic array.
#define da_append(da, item) do { \
    if ((da)->len >= (da)->cap) { \
        (da)->cap = (da)->cap == 0 ? DA_DEFAULT_CAP : (da)->cap * 2; \
        (da)->data = realloc((da)->data, (da)->cap * sizeof(*(da)->data)); \
        if (!(da)->data) abort(); \
    } \
    (da)->data[(da)->len++] = (item); \
} while (0)

// Append n items to the dynamic array.
#define da_appendn(da, items, n) do { \
    size_t len = n; \
    for (size_t i = 0; i < len; i++) { \
        da_append(da, items[i]); \
    } \
} while (0)


// A readonly view of a string.
typedef struct
{
    const char *data;
    size_t len;
} StringView;

// A mutable, owning string.
typedef struct
{
    char *data;
    size_t len;
    size_t cap;
} String;

StringView _sv_from_lit(const void *p, size_t n);
StringView _sv_from_cstrp(const void *pp);
StringView _sv_from_str(const void *p);
StringView _sv_from_strp(const void *pp);

// Convert string literal, char pointer, or string to a string view.
#define SV(s) _Generic(&(s), \
    char (*)[sizeof(s)]: _sv_from_lit(&(s), sizeof(s)), \
    char **:             _sv_from_cstrp(&(s)), \
    const char **:       _sv_from_cstrp(&(s)), \
    String *:            _sv_from_str(&(s)), \
    const String *:      _sv_from_str(&(s)), \
    String **:           _sv_from_strp(&(s)), \
    const String **:     _sv_from_strp(&(s)))

// Declares a char array named `id`.
#define SV_TO_CSTR(sv, id) \
    char id[(sv).len]; \
    memcpy(&id, (sv).data, (sv).len); \
    id[(sv).len] = '\0';

// Returns if a and b are equal.
bool sv_equal(StringView a, StringView b);

#define SV_FMT "%.*s"
#define SV_ARG(s) ((int)(s).len), ((s).data)

// Append a string view to the string.
#define string_append_view(s, value) do { \
    da_appendn((s), (value).data, (value).len); \
    (s)->data[(s)->len] = '\0'; \
} while (0)

// Append a C-string to the string.
#define string_append_cstr(s, value) do { \
    da_appendn((s), (value), strlen(value)); \
    (s)->data[(s)->len] = '\0'; \
} while (0)

// Append a formatted string to the string.
void string_appendf(String *s, const char *fmt, ...);

// Append a variadic formatted string to the string.
void string_appendvf(String *s, const char *fmt, va_list args);

#define string_init(s) do { \
    da_init(s); \
    (s)->data[0] = '\0'; \
} while (0)

#define string_reserve(s, n) do { \
    da_reserve((s), (n)); \
    (s)->data[0] = '\0'; \
} while (0)

#define string_free(s) da_free(s)

#define string_reset(s) do { \
    da_reset(s); \
    (s)->data[0] = '\0'; \
} while (0)


/************************************************
 * Logging
 ************************************************/

// Levels of a log.
typedef enum
{
    CUT_LOG_INFO,
    CUT_LOG_DEBUG,
    CUT_LOG_ERROR,
    CUT_LOG_FATAL,
} CutLogLevel;

// A log message.
typedef struct
{
    CutLogLevel level;
    int line;
    String *message;
    StringView file;
} CutLog;

// A dynamic array of logs.
typedef struct
{
    CutLog *data;
    size_t len;
    size_t cap;
} CutLogList;

// Append a log from within a test case.
void _cut_log_append(CutLogList *logs, CutLog log, const char *fmt, ...);
#define cut_log_append(lvl, msg, ...) _cut_log_append((_logs), \
    (CutLog){.level=(lvl), .line=__LINE__, .file=SV(__FILE__)}, \
    (msg) __VA_OPT__(,) __VA_ARGS__)

// Append a log to the global list.
void _cut_dev_log_append(CutLog log, const char *fmt, ...);
#define cut_dev_log_append(lvl, msg, ...) _cut_dev_log_append((CutLog){ \
        .level=(lvl), .line=__LINE__, .file=SV(__FILE__)}, \
        (msg) __VA_OPT__(,) __VA_ARGS__)

#define TEST_LOG_INFO(msg, ...)  cut_log_append(CUT_LOG_INFO,  (msg),  __VA_ARGS__)
#define TEST_LOG_DEBUG(msg, ...) cut_log_append(CUT_LOG_DEBUG, (msg),  __VA_ARGS__)
#define TEST_LOG_ERROR(msg, ...) cut_log_append(CUT_LOG_ERROR, (msg),  __VA_ARGS__)
#define TEST_LOG_FATAL(msg, ...) cut_log_append(CUT_LOG_FATAL, (msg),  __VA_ARGS__); return

#define DEV_LOG_INFO(msg, ...)  cut_dev_log_append(CUT_LOG_INFO,  (msg),  __VA_ARGS__); cut_dev_log_print()
#define DEV_LOG_DEBUG(msg, ...) cut_dev_log_append(CUT_LOG_DEBUG, (msg),  __VA_ARGS__); cut_dev_log_print()
#define DEV_LOG_ERROR(msg, ...) cut_dev_log_append(CUT_LOG_ERROR, (msg),  __VA_ARGS__); cut_dev_log_print()
#define DEV_LOG_FATAL(msg, ...) cut_dev_log_append(CUT_LOG_FATAL, (msg),  __VA_ARGS__); cut_dev_log_print(); abort()

void cut_dev_log_print(void);

/************************************************
 * Unit Testing
 ************************************************/

// A test function run by a TestCase.
typedef void (*CutTestFn)(CutLogList *);

// A test case. Node in a linked list.
typedef struct CutTestCase
{
    StringView name;
    CutTestFn run;
    StringView file;
    int line;
    struct CutTestCase *next;
} CutTestCase;

// Add a test case to the global registry.
void cut_test_registry_add(StringView name, CutTestFn fn, StringView file, int line);

#define TEST(name) \
    static void name(CutLogList *_logs); \
    static void __attribute__((constructor)) _register_test_##name(void) { \
        cut_test_registry_add(SV(#name), name, SV(__FILE__), __LINE__); \
    } \
    static void name(CutLogList *_logs)

// Options for a test run.
typedef struct
{
    // The file descriptor to output to.
    FILE *fdout;
} TestRunOpt;

// Run tests with options.
void cut_test_run_opt(TestRunOpt opt);
#define cut_test_run(...) cut_test_run_opt((TestRunOpt){ \
        .fdout=stdout, __VA_ARGS__})

// Alias for the main function to run all tests.
#define TEST_RUN() int main(void) { cut_test_run(); return 0; }


/************************************************
 * Test/Dev Features
 ************************************************/

#define CUT_DEBUG(fmt, ...) do { \
    TEST_LOG_DEBUG(fmt, __VA_ARGS__); \
} while (0)

#define CUT_CHECK(exp) do { \
    if (!(exp)) { TEST_LOG_ERROR(#exp); } \
} while (0)

#define CUT_ERROR(fmt, ...) do { \
    TEST_LOG_ERROR(fmt, __VA_ARGS__); \
} while (0)

#define CUT_MUST(exp) do { \
    if (!(exp)) { TEST_LOG_FATAL(#exp); } \
} while (0)

#define CUT_FATAL(fmt, ...) do { \
    TEST_LOG_FATAL(fmt, __VA_ARGS__); \
} while (0)


#ifdef CUT_NO_DEV

#define DEV_INFO(fmt, ...)     do {} while (0)
#define DEV_DEBUG(fmt, ...)    do {} while (0)
#define DEV_CHECK(exp)         do {} while (0)
#define DEV_ERROR(fmt, ...)    do {} while (0)
#define DEV_MUST(exp)          do {} while (0)
#define DEV_FATAL(fmt, ...)    do {} while (0)
#define TODO(msg)              do {} while (0)
#define UNREACHABLE()          do {} while (0)

#else

#define DEV_INFO(fmt, ...) do { \
    DEV_LOG_INFO(fmt, __VA_ARGS__); \
} while (0)

#define DEV_DEBUG(fmt, ...) do { \
    DEV_LOG_DEBUG(fmt, __VA_ARGS__); \
} while (0)

#define DEV_CHECK(exp) do {\
    if (!(exp)) { DEV_LOG_ERROR(#exp); } \
} while (0)

#define DEV_ERROR(fmt, ...)  do { \
    DEV_LOG_ERROR(fmt, __VA_ARGS__); \
} while (0)

#define DEV_MUST(exp) do {\
    if (!(exp)) { DEV_LOG_FATAL(#exp); } \
} while (0)

#define DEV_FATAL(fmt, ...) do { \
    DEV_LOG_FATAL(fmt, __VA_ARGS__);  \
} while (0)

#define TODO(msg) do { \
    DEV_LOG_DEBUG("TODO: "msg); \
} while (0)

#define UNREACHABLE() do { \
    DEV_LOG_FATAL("unreachable code block"); \
} while (0)
#endif // CUT_NO_DEV


/************************************************
 * Build
 ************************************************/

// A dynamic array of string views.
// Assuming static lifetime for the data.
typedef struct
{
    StringView *data;
    size_t len;
    size_t cap;
} SVList;

// The kinds of a unit.
typedef enum
{
    CUT_UNIT_EXE,
    CUT_UNIT_LIB,
} CutUnitKind;

// A build unit.
typedef struct
{
    StringView name;
    CutUnitKind kind;

    SVList sources;
    SVList includes;
    SVList flags;
    SVList defines;
    SVList libs;
    SVList lib_dirs;
} CutUnit;

// Initializes a unit.
void cut_unit_init(CutUnit *unit, const char *name, CutUnitKind kind);

// Collect variadic list of strings into a list.
void _cut_make_sv_list(SVList *sl, ...);

#define cut_unit_sources(unit, ...)  _cut_make_sv_list(&(unit)->sources,  __VA_OPT__(__VA_ARGS__,) NULL);
#define cut_unit_includes(unit, ...) _cut_make_sv_list(&(unit)->includes, __VA_OPT__(__VA_ARGS__,) NULL);
#define cut_unit_flags(unit, ...)    _cut_make_sv_list(&(unit)->flags,    __VA_OPT__(__VA_ARGS__,) NULL);
#define cut_unit_defines(unit, ...)  _cut_make_sv_list(&(unit)->defines,  __VA_OPT__(__VA_ARGS__,) NULL);
#define cut_unit_libs(unit, ...)     _cut_make_sv_list(&(unit)->libs,     __VA_OPT__(__VA_ARGS__,) NULL);
#define cut_unit_lib_dirs(unit, ...) _cut_make_sv_list(&(unit)->lib_dirs, __VA_OPT__(__VA_ARGS__,) NULL);

// Options for builder
typedef struct
{
    StringView cc;
    StringView build_dir;
    StringView script_name;
} CutBuilderOpt;

typedef struct
{
    StringView cc;
    StringView build_dir;
    StringView script_name;
    StringView file;
    CutUnit **units;
    size_t units_len;
} CutBuilder;

#define CC_DEFAULT        SV("cc")
#define BUILD_DIR_DEFAULT SV("build")

// Initialize the build with options.
void cut_build_init_opt(StringView file, CutBuilderOpt opt);
#define cut_build_init(...) cut_build_init_opt(SV(__FILE__), \
    (CutBuilderOpt){.cc=CC_DEFAULT, .build_dir=BUILD_DIR_DEFAULT, \
    .script_name=(StringView){__FILE__, sizeof(__FILE__)-3}, \
    __VA_ARGS__})

// Define all units / targets for the build.
void _cut_build_add(CutUnit *first, ...);
#define cut_build_add(first, ...) _cut_build_add(first, __VA_OPT__(__VA_ARGS__,) NULL);

// Run build.
int cut_build_run(int argc, char **argv);


#endif // CUT_H


/************************************************
 * Implementation
 ************************************************/

// #define CUT_IMPL
#ifdef CUT_IMPL

#include <assert.h>
#include <stddef.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef _WIN32

#include <io.h>
#include <windows.h>
#include <direct.h>

#define isatty     _isatty
#define fileno     _fileno
#define stat       _stat

#define makedir(x) _mkdir(x)

#else

#include <unistd.h>

#define makedir(x) mkdir(x, 0755)

#endif // _WIN32


/************************************************
 * Globals
 ************************************************/

// Global linked list of test cases.
static CutTestCase *cut_test_registry_head = NULL;

// Count of registered tests.
static size_t cut_test_registry_size = 0;

// Length of the longest test name
static size_t cut_test_name_max = 0;

// Global test context.
static CutLogList cut_dev_logs = {0};

// Global builder.
static CutBuilder cut_builder = {0};


/************************************************
 * String Utils
 ************************************************/

StringView _sv_from_lit(const void *p, size_t n)
{
    return (StringView){(const char *)p, n-1};
}

StringView _sv_from_cstrp(const void *pp)
{
    const char *const *s = pp;
    return (StringView){*s, strlen(*s)};
}

StringView _sv_from_str(const void *p)
{
    const String *s = p;
    return (StringView){s->data, s->len};
}

StringView _sv_from_strp(const void *pp)
{
    const String *const *s = pp;
    return (StringView){(*s)->data, (*s)->len};
}

// Returns if a and b are equal.
bool sv_equal(StringView a, StringView b)
{
    if (a.len != b.len) return false;

    for (size_t i = 0; i < a.len; i++)
        if (a.data[i] != b.data[i]) return false;

    return true;
}

// Append a formatted string to the string.
void string_appendf(String *s, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    string_appendvf(s, fmt, args);
    va_end(args);
}

// Append a variadic formatted string to the string.
void string_appendvf(String *s, const char *fmt, va_list args)
{
    va_list copy;
    va_copy(copy, args);
    int size = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if (size < 0) return;
    char buffer[size+1];
    vsnprintf(buffer, (size_t)size+1, fmt, args);
    string_append_cstr(s, buffer);
}

// Format the string list into a whitespace-separated string.
static void string_list_format(SVList *sl, String *sb, StringView prefix)
{
    da_for(sl, i)
    {
        string_appendf(sb, SV_FMT, SV_ARG(prefix));
        StringView sv = sl->data[i];
        string_appendf(sb, SV_FMT" ", SV_ARG(sv));
    }
}

/************************************************
 * Platform Utils
 ************************************************/

#ifdef _WIN32
#define PATH_SEP "\\"
#else
#define PATH_SEP "/"
#endif

// Get the mtime of a file, return -1 if error.
static int get_mtime(StringView path, time_t *mtime)
{
    SV_TO_CSTR(path, path_buf);

    struct stat st;
    if (stat(path_buf, &st) != 0)
        return -1;

    *mtime = st.st_mtime;
    return 0;
}

typedef enum
{
    MKDIR_CREATED,
    MKDIR_EXISTS,
    MKDIR_FAILED,
} MkdirResult;

// Create a directory if it doesn't exist.
static MkdirResult mkdir_if_not_exist(StringView path)
{
    SV_TO_CSTR(path, path_buf);

    if (makedir(path_buf) == 0)
        return MKDIR_CREATED;

    if (errno == EEXIST)
        return MKDIR_EXISTS;

    return MKDIR_FAILED;
}

// Append the name of the executable to the string builder.
static void append_exe_name(String *sb, StringView base)
{
    string_append_view(sb, base);

#ifdef _WIN32
    string_appendf(sb, ".exe");
#endif
}

// Generate the build command for a unit.
static void generate_build_command(CutUnit *unit, String *sb)
{
    string_appendf(sb, SV_FMT" ", SV_ARG(cut_builder.cc));

    string_list_format(&unit->sources, sb, SV(""));
    string_list_format(&unit->includes, sb, SV("-I"));
    string_list_format(&unit->flags, sb, SV(""));
    string_list_format(&unit->defines, sb, SV("-D"));
    string_list_format(&unit->libs, sb, SV("-l"));

    string_appendf(sb, "-o "SV_FMT"/", SV_ARG(cut_builder.build_dir));
    string_appendf(sb, SV_FMT" ", SV_ARG(unit->name));
}

// Generate the command to run an executable. Contains a trailing whitespace.
static void generate_run_command(StringView name, StringView parent, String *sb)
{
    string_appendf(sb, "."PATH_SEP);
    if (parent.len > 0)
        string_appendf(sb, SV_FMT PATH_SEP, SV_ARG(cut_builder.build_dir));

    append_exe_name(sb, name);
    string_appendf(sb, " ");
}

// Run a external command.
static void exec_command(StringView cmd)
{
    DEV_INFO(SV_FMT, SV_ARG(cmd));

    SV_TO_CSTR(cmd, cmd_buf);

    if (system(cmd_buf) != 0)
        DEV_FATAL("Failed to execute command.");
}

// Get the backup name for the script executable.
static void old_script_exe_name(String *sb)
{
    append_exe_name(sb, cut_builder.script_name);
    string_appendf(sb, ".old");
}

// Remove a path.
static void remove_path(StringView path)
{
    String cmd;
    string_init(&cmd);

#ifdef _WIN32
    string_appendf(&cmd, "del /q /s ");
#else
    string_appendf(&cmd, "rm -rf ");
#endif
    string_appendf(&cmd, "\""SV_FMT"\"", SV_ARG(path));
    exec_command(SV(cmd));
    string_free(&cmd);
}

/************************************************
 * Logging
 ************************************************/

// ANSI colors
#define COLOR_RCT   "\033[0m"
#define COLOR_FN    "\033[36m"
#define COLOR_OK    "\033[32m"
#define COLOR_SUB   "\033[2m"
#define COLOR_DEBUG "\033[0m"
#define COLOR_ERROR "\033[33m"
#define COLOR_FATAL "\033[1;31m"

static void log_free(CutLog *log)
{
    da_free(log->message);
}

// Reset a log list.
static void log_list_reset(CutLogList *logs)
{
    if (logs->data)
        da_map_mut(logs, CutLog, log_free);
    da_reset(logs);
}

// Append a log with a formatted message.
static void log_list_append(CutLogList *logs, CutLog log, const char *fmt, va_list args)
{
    assert(logs);

    log.message = malloc(sizeof(String));
    string_init(log.message);
    assert(log.message);
    string_appendvf(log.message, fmt, args);

    da_append(logs, log);
}

// Append a log from within a test case.
void _cut_log_append(CutLogList *logs, CutLog log, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_list_append(logs, log, fmt, args);
    va_end(args);
}

// Append a log to the global list.
void _cut_dev_log_append(CutLog log, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    log_list_append(&cut_dev_logs, log, fmt, args);
    va_end(args);
}

// Print all logs to fdout with optional prefix.
static void log_list_print(CutLogList *logs, FILE *fdout, const char *prefix)
{
    bool is_ansi = isatty(fileno(fdout));
    if (!prefix) prefix = "";

    for (size_t ei = 0; ei < logs->len; ei++)
    {
        CutLog log = logs->data[ei];

        fprintf(fdout, "%s", prefix);

        if (is_ansi) fprintf(fdout, COLOR_SUB);
        fprintf(fdout, "["SV_FMT":%d] ", SV_ARG(log.file), log.line);
        if (is_ansi) fprintf(fdout, COLOR_RCT);

        switch (log.level)
        {
            case CUT_LOG_INFO: 
                fprintf(fdout, "[INFO] "); 
                break;

            case CUT_LOG_DEBUG: 
                if (is_ansi) fprintf(fdout, COLOR_DEBUG);
                fprintf(fdout, "[DEBUG] "); 
                break;

            case CUT_LOG_ERROR: 
                if (is_ansi) fprintf(fdout, COLOR_ERROR);
                fprintf(fdout, "[ERROR] "); 
                break;

            case CUT_LOG_FATAL: 
                if (is_ansi) fprintf(fdout, COLOR_FATAL);
                fprintf(fdout, "[FATAL] "); 
                break;
        }
        fprintf(fdout, SV_FMT"\n", SV_ARG(*log.message));
        if (is_ansi) fprintf(fdout, COLOR_RCT);
    }
}

// Print the current dev log and reset.
void cut_dev_log_print(void)
{
    if (cut_dev_logs.len == 0)
        da_init(&cut_dev_logs);
    if (cut_test_registry_size > 0) return;
    log_list_print(&cut_dev_logs, stdout, NULL);
    log_list_reset(&cut_dev_logs);
}


/************************************************
 * Unit Testing
 ************************************************/

// Add a test case to the global registry.
void cut_test_registry_add(StringView name, CutTestFn fn, StringView file, int line)
{
    CutTestCase *new_test = malloc(sizeof(CutTestCase));
    new_test->file = file;
    new_test->line = line;
    new_test->run = fn;
    new_test->name = name;

    new_test->next = cut_test_registry_head;
    cut_test_registry_head = new_test;
    cut_test_registry_size++;

    if (name.len > cut_test_name_max)
        cut_test_name_max = name.len;
}

// Flatten the linked list of test cases into an array.
static bool test_registry_flatten(CutTestCase *head, size_t size, CutTestCase *out[size])
{
    size_t i = 0;
    while (head && i < size)
    {
        out[i++] = head;
        head = head->next;
    }

    return i == size && head == NULL;
}

// Print information of a test case.
static void test_case_info_print(CutTestCase *test, FILE *fdout, bool ansi)
{
    if (ansi) fprintf(fdout, COLOR_SUB);
    fprintf(fdout, "["SV_FMT":%d] ", SV_ARG(test->file), test->line);
    if (ansi) fprintf(fdout, COLOR_RCT);

    if (ansi) fprintf(fdout, COLOR_FN);
    fprintf(fdout, SV_FMT, SV_ARG(test->name));
    if (ansi) fprintf(fdout, COLOR_RCT);

    for (size_t i = test->name.len; i < cut_test_name_max; i++)
        fprintf(fdout, " ");

    fprintf(fdout, " ... ");
}

// Print the status of a test's logs and return if it failed.
static bool test_case_status_print(CutLogList *logs, FILE* fdout, bool ansi)
{
    size_t failure_count = 0;
    da_for(logs, i)
    {
        CutLog log = logs->data[i];
        if (log.level == CUT_LOG_ERROR || log.level == CUT_LOG_FATAL)
            failure_count++;
    }

    if (failure_count > 0)
    {
        if (ansi) fprintf(fdout, COLOR_FATAL);
        fprintf(fdout, "failed\n");
        if (ansi) fprintf(fdout, COLOR_RCT);
        return true;
    }
    else
    {
        if (ansi) fprintf(fdout, COLOR_OK);
        fprintf(fdout, "passed\n");
        if (ansi) fprintf(fdout, COLOR_RCT);
        return false;
    }
}

// Run tests with options.
void cut_test_run_opt(TestRunOpt opt)
{
    if (cut_test_registry_size <= 0)
    {
        fprintf(opt.fdout, "No tests to run.");
        return;
    }

    bool is_ansi = isatty(fileno(opt.fdout));

    size_t test_ran = 0;
    size_t test_failed = 0;

    size_t total = cut_test_registry_size;
    CutTestCase *all_tests[total];

    assert(test_registry_flatten(cut_test_registry_head, total, all_tests));

    CutLogList test_logs;
    da_init(&test_logs);
    da_init(&cut_dev_logs);

    for (size_t ti = 0; ti < total; ti++)
    {
        CutTestCase *test = all_tests[ti];
        if (!test) continue;

        test_case_info_print(test, opt.fdout, is_ansi);

        test_ran++;
        test->run(&test_logs);

        if (test_case_status_print(&test_logs, opt.fdout, is_ansi))
            test_failed++;

        log_list_print(&test_logs, opt.fdout, "    ");
        log_list_reset(&test_logs);

        if (cut_dev_logs.len > 0)
        {
            // Print any dev logs emitted during the test.
            // Ideally they should be printed as emitted, but they are ephemeral.
            log_list_print(&cut_dev_logs, opt.fdout, "    ");
            log_list_reset(&cut_dev_logs);
        }

        free(test);
    }

    fprintf(opt.fdout, "\nTotal: %zu, passed: %zu, failed: %zu\n", 
            test_ran, test_ran-test_failed, test_failed);
}


/************************************************
 * Build
 ************************************************/

// Initializes a unit.
void cut_unit_init(CutUnit *unit, const char *name, CutUnitKind kind)
{
    assert(unit);
    memset(unit, 0, sizeof(*unit));

    unit->name = SV(name);
    unit->kind = kind;
}

// Collect strings from a variadic list of strings.
void _cut_make_sv_list(SVList *sl, ...)
{
    va_list args;
    va_start(args, sl);

    if (sl->len == 0) da_init(sl);

    const char *current = va_arg(args, const char *);
    while (current)
    {
        StringView sv = SV(current);
        da_append(sl, sv);
        current = va_arg(args, const char *);
    }

    va_end(args);
}

// Checks whether the build script needs rebuilding.
static bool should_rebuild(StringView file, StringView exe_name)
{
    time_t mtime_file, mtime_exe;
    String exe;
    string_init(&exe);
    append_exe_name(&exe, exe_name);

    if (get_mtime(file, &mtime_file) != 0)
        return true;

    if (get_mtime(SV(exe), &mtime_exe) != 0)
        return true;

    return difftime(mtime_file, mtime_exe) > 0;
}

// Rebuild the build script.
static void cut_rebuild(size_t argc, StringView *argv)
{
    String sb;
    string_init(&sb);
    append_exe_name(&sb, cut_builder.script_name);

    String new_path;
    string_init(&new_path);
    old_script_exe_name(&new_path);

    DEV_INFO("Renaming '"SV_FMT"' to '"SV_FMT"'",
            SV_ARG(sb), SV_ARG(new_path));

    bool ok = false;
#ifdef _WIN32
    ok = MoveFileExA(sb.data, new_path.data, MOVEFILE_REPLACE_EXISTING);
#else
    ok = rename(sb.data, new_path.data) == 0;
#endif
    if (!ok)
        DEV_FATAL("Failed to rename '"SV_FMT"'", SV_ARG(sb));

    string_free(&new_path);

    string_reset(&sb);

    string_appendf(&sb, SV_FMT" ",    SV_ARG(cut_builder.cc));
    string_appendf(&sb, SV_FMT" ",    SV_ARG(cut_builder.file));
    string_appendf(&sb, "-o "SV_FMT,  SV_ARG(cut_builder.script_name));
    exec_command(SV(sb));

    string_reset(&sb);
    generate_run_command(cut_builder.script_name, SV(""), &sb);
    for (size_t i = 1; i < argc; i++)
        string_appendf(&sb, SV_FMT" ", SV_ARG(argv[i]));

    exec_command(SV(sb));

    exit(0);
}

// Initialize the build with options.
void cut_build_init_opt(StringView file, CutBuilderOpt opt)
{
    cut_builder.cc = opt.cc;
    cut_builder.build_dir = opt.build_dir;
    cut_builder.script_name = opt.script_name;
    cut_builder.file = file;

    switch (mkdir_if_not_exist(opt.build_dir))
    {
        case MKDIR_CREATED:
            DEV_INFO("Created directory: "SV_FMT, SV_ARG(opt.build_dir));
            break;

        case MKDIR_EXISTS:
            break;

        case MKDIR_FAILED:
            DEV_FATAL("Unknown error occured during directory creation.");
    }
}

// Define all units for the build.
void _cut_build_add(CutUnit *first, ...)
{
    assert(first);

    va_list args, copy;
    va_start(args, first);
    va_copy(copy, args);

    size_t count = 1;
    CutUnit *current = first;
    while (current)
    {
        count++;
        current = va_arg(copy, CutUnit *);
    }
    va_end(copy);

    cut_builder.units = malloc(count * sizeof(CutUnit *));
    current = first;
    while (current)
    {
        cut_builder.units[cut_builder.units_len++] = current;
        current = va_arg(args, CutUnit *);
    }
    va_end(args);
}


// Find a unit by name.
static CutUnit *cut_build_find_unit(StringView name)
{
    for (size_t i = 0; i < cut_builder.units_len; i++)
    {
        CutUnit *u = cut_builder.units[i];
        if (sv_equal(name, u->name))
            return u;
    }
    return NULL;
}

// Run build.
int cut_build_run(int argc, char **argv)
{
    String cmd;
    string_init(&cmd);

    StringView args[argc];
    for (int i = 0; i < argc; i++)
        args[i] = SV(argv[i]);

    if (should_rebuild(cut_builder.file, cut_builder.script_name))
        cut_rebuild(argc, args);

    if (argc == 2 && sv_equal(args[1], SV("clean")))
    {
        string_appendf(&cmd, SV_FMT PATH_SEP "*", SV_ARG(cut_builder.build_dir));
        remove_path(SV(cmd));

        string_reset(&cmd);
        old_script_exe_name(&cmd);
        remove_path(SV(cmd));
        return 0;
    }

    if (argc >= 3 && sv_equal(args[1], SV("rebuild")))
    {
        cut_rebuild(argc-1, args+1);
        return 0;
    }

    if (argc >= 3)
    {
        if (sv_equal(args[1], SV("build")) || sv_equal(args[1], SV("run")))
        {
            CutUnit *unit = NULL;
            unit = cut_build_find_unit(args[2]);
            if (!unit) 
                DEV_FATAL("Unit '"SV_FMT"' does not exist.", SV_ARG(args[2]));

            generate_build_command(unit, &cmd);
            exec_command(SV(cmd));

            if (sv_equal(args[1], SV("run")))
            {
                string_reset(&cmd);
                generate_run_command(unit->name, cut_builder.build_dir, &cmd);
                exec_command(SV(cmd));
            }
            return 0;
        }
    }

    printf("Usage: \n"
           "    <cut> help             show this help\n"
           "    <cut> build <name>     build the unit\n"
           "    <cut> run <name>       build and run the unit\n"
           "    <cut> clean            clean artifacts\n"
           "    <cut> rebuild <cmd>    rebuild script executable\n");

    return 1;
}

#endif // CUT_IMPL
