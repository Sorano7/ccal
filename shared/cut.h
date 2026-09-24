#ifndef CUT_H
#define CUT_H

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>

/************************************************
 * Dynamic Array
 ************************************************/

// Default capacity for any dynamic array.
#define DA_DEFAULT_CAP 64

#define DA_DEFINE(name, T) typedef struct name { T *data; size_t len, cap; } name

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

#define da_at(da, i) (da)->data[(i)]
#define da_last(da)  (da)->data[(da)->len-1]

#define DA_FOR(da, i) for (size_t i = 0; i < (da)->len; i++)
#define DA_FOREACH(da, T, it) for (T *it = (da)->data; it < (da)->data+(da)->len; it++)

// Map a function accepting and returning type to each item in the dynamic array.
#define da_map(da, type, f) do { \
    DA_FOR((da), i) { \
        type item = da_at((da), i); \
        da_at((da), i) = (f)(item); \
    } \
} while (0)

// Map a function accepting pointer to type to each item in the dynamic array.
#define da_map_mut(da, type, f) do { \
    DA_FOR((da), i) { \
        (f)((type *)(&(da_at((da), i)))); \
    } \
} while (0)

#define da_grow(da) do { \
    if ((da)->len >= (da)->cap) { \
        (da)->cap = (da)->cap == 0 ? DA_DEFAULT_CAP : (da)->cap * 2; \
        (da)->data = realloc((da)->data, (da)->cap * sizeof(*(da)->data)); \
        if (!(da)->data) abort(); \
    } \
} while (0)

#define da_resize(da, c) do { \
    if ((da)->cap < (c)) { \
        (da)->cap = (c); \
        (da)->data = realloc((da)->data, (da)->cap * sizeof(*(da)->data)); \
        if (!(da)->data) abort(); \
    } \
} while (0)

// Append an item to the dynamic array.
#define da_append(da, item) do { \
    da_grow(da); \
    (da)->data[(da)->len++] = (item); \
} while (0)

// Append n items to the dynamic array.
#define da_appendn(da, items, n) do { \
    size_t len = n; \
    for (size_t i = 0; i < len; i++) { \
        da_append(da, items[i]); \
    } \
} while (0)

#define da_clone(da, from) do { \
    da_reserve((da), (from)->len); \
    da_appendn((da), (from)->data, (from)->len); \
} while (0)

/************************************************
 * Strings
 ************************************************/

// A readonly view of a string.
typedef struct
{
    const char *data;
    size_t len;
} StringView;

// A mutable, owning string.
DA_DEFINE(String, char);

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
    const String **:     _sv_from_strp(&(s)), \
    StringView *:        (s), \
    const StringView *:  (s))

// Declares a char array named `id`.
#define SV_TO_CSTR(sv, id) \
    char id[(sv).len+1]; \
    memcpy(&id, (sv).data, (sv).len); \
    id[(sv).len] = '\0';

char *sv_alloc_cstr(StringView s);

// Returns if a and b are equal.
bool _sv_equal(StringView a, StringView b);

#define sv_equal(a, b) _sv_equal(SV(a), SV(b))

#define SV_FMT "%.*s"
#define SV_ARG(s) ((int)(s).len), ((s).data)

#define str_append_null(s) do { \
    da_grow(s); \
    (s)->data[(s)->len] = '\0'; \
} while (0)

void str_append_char(String *s, char v);
void str_append_view(String *s, StringView v);
void str_append_str(String *s, const String *v);
void str_append_cstr(String *s, const char *v);

#define str_append(s, v) _Generic((v), \
    char:           str_append_char, \
    char *:         str_append_cstr, \
    const char *:   str_append_cstr, \
    StringView:     str_append_view, \
    String *:       str_append_str, \
    const String *: str_append_str \
)(s, v)

// Append a formatted string to the string.
void str_appendf(String *s, const char *fmt, ...);

// Append a variadic formatted string to the string.
void str_appendvf(String *s, const char *fmt, va_list args);

// Initializes a string.
#define str_init(s) do { \
    da_init(s); \
    str_append_null(s); \
} while (0)

// Initializes a string with a value
#define str_init_with(s, v) do { \
    da_init(s); \
    str_append(s, v); \
} while (0)

// Initializes a string with a reserved capacity.
#define str_reserve(s, n) do { \
    da_reserve((s), (n)); \
    str_append_null(s); \
} while (0)

// Free a string.
#define str_free(s) da_free(s)

// Reset a string.
#define str_reset(s) do { \
    da_reset(s); \
    str_append_null(s); \
} while (0)

#define str_clone(s, from) do { \
    str_reserve((s), (from)->len+1); \
    str_append((s), (from)); \
} while (0)

// Insert an element at index n.
void str_insert(String *s, char v, size_t n);

// Find the index of a character.
size_t sv_find(StringView s, char v);

// Shift a string view by n. Mutates input.
StringView sv_shift(StringView *s, size_t n);

// Shift a string view until the next instance of delim. Mutates input.
StringView sv_split(StringView *s, char delim);

// Trim whitespaces from the string view.
StringView sv_trim(StringView s);

typedef struct
{
    size_t from;
    size_t to;
} SVSLiceOpt;

// Return a slice of the string view.
StringView sv_slice_opt(StringView s, SVSLiceOpt opt);

#define sv_slice(s, ...) sv_slice_opt((s), (SVSLiceOpt){ \
        .from=0, .to=SIZE_MAX, __VA_ARGS__})

// Read a full line from the file stream.
void str_readline(String *s, FILE *stream);

// Read the content of a file.
bool str_readfile(String *s, FILE *file);

// Check if the string view starts with a prefix.
bool sv_startswith(StringView s, StringView prefix);

// Check if the string view ends with a suffix.
bool sv_endswith(StringView s, StringView suffix);

// Convert a string view to int.
bool sv_to_int(StringView s, int *out);

// A dynamic array of string views.
DA_DEFINE(SVList, StringView);

void svlist_join(SVList *sv, String *sb, StringView delim);
bool svlist_contains(SVList *sl, StringView v);

/************************************************
 * Logging
 ************************************************/

#define AFMT_RESET     "\e[0m"
#define AFMT_BOLD      "\e[1m"
#define AFMT_DIM       "\e[2m"
#define AFMT_ITATLIC   "\e[3m"
#define AFMT_UNDERLINE "\e[4m"
#define AFMT_BLINK     "\e[5m"
#define AFMT_REVERSE   "\e[7m"
#define AFMT_STRIKE    "\e[9m"

#define ACOLOR_BLACK   "\e[30m"
#define ACOLOR_RED     "\e[31m"
#define ACOLOR_GREEN   "\e[32m"
#define ACOLOR_YELLOW  "\e[33m"
#define ACOLOR_BLUE    "\e[34m"
#define ACOLOR_MAGENTA "\e[35m"
#define ACOLOR_CYAN    "\e[36m"
#define ACOLOR_WHITE   "\e[37m"

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
DA_DEFINE(CutLogList, CutLog);

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

// The kinds of a unit.
typedef enum
{
    CUT_UNIT_EXE,
    CUT_UNIT_LIB_SHARED,
    CUT_UNIT_LIB_STATIC,
} CutUnitKind;

// A build unit.
typedef struct
{
    StringView name;
    StringView libname;
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

#define cut_unit_lib_name(unit, name) do { \
    (unit)->libname = SV(name); \
} while (0)

// Options for builder
typedef struct
{
    StringView cc;
    StringView build_dir;
    StringView lib_dir;
    StringView script_name;
} CutBuilderOpt;

typedef struct
{
    StringView cc;
    StringView build_dir;
    StringView lib_dir;
    StringView script_name;
    StringView file;
    CutUnit **units;
    size_t units_len;
} CutBuilder;

#define CC_DEFAULT        SV("cc")
#define BUILD_DIR_DEFAULT SV("build")
#define LIB_DIR_DEFAULT   SV("lib")

// Initialize the build with options.
void cut_build_init_opt(StringView file, CutBuilderOpt opt);
#define cut_build_init(...) cut_build_init_opt(SV(__FILE__), \
    (CutBuilderOpt){.cc=CC_DEFAULT, .build_dir=BUILD_DIR_DEFAULT, \
    .lib_dir=LIB_DIR_DEFAULT, \
    .script_name=(StringView){__FILE__, sizeof(__FILE__)-3}, \
    __VA_ARGS__})

// Define all units / targets for the build.
void _cut_build_add(CutUnit *first, ...);
#define cut_build_add(first, ...) _cut_build_add(first, __VA_OPT__(__VA_ARGS__,) NULL);

// Run build.
int cut_build_run(int argc, char **argv);


/************************************************
 * CLI Flag Parsing
 ************************************************/

typedef enum
{
    CUT_FLAG_BOOL,
    CUT_FLAG_INT,
    CUT_FLAG_STR,
} CutFlagKind;

typedef struct
{
    void *data;
    StringView desc;
    StringView long_name;
    char short_name;
    CutFlagKind kind;
} CutFlag;

DA_DEFINE(CutFlagList, CutFlag);

typedef struct
{
    SVList commands;
    CutFlagList optional;
} CutFlagParser;

typedef struct
{
    StringView desc;
    char short_name;
} CutFlagOpt;

// Initialize a flag parser.
void cut_fp_init(CutFlagParser *fp);

// Reset a flag parser's configuration.
void cut_fp_reset(CutFlagParser *fp);

// Free a flag parser.
void cut_fp_free(CutFlagParser *fp);

// Add a command to the flag parser.
void cut_fp_add_command(CutFlagParser *fp, StringView cmd);

// Add an optional flag to the flag parser.
void cut_fp_add_flag_opt(CutFlagParser *fp, CutFlagKind kind, 
        void *data, StringView name, CutFlagOpt opt);

#define cut_fp_add_flag(fp, data, name, ...) cut_fp_add_flag_opt((fp), \
    _Generic((data), \
        bool *:       CUT_FLAG_BOOL, \
        int *:        CUT_FLAG_INT, \
        StringView *: CUT_FLAG_STR), \
    (data), (name), (CutFlagOpt){ \
        .desc=SV(""), .short_name=0, \
    __VA_ARGS__})

typedef enum
{
    CUT_FP_OK,
    CUT_FP_INVALID_VALUE,
    CUT_FP_MISSING_VALUE,
} CutFPStatus;

typedef struct
{
    String *msg;
    CutFPStatus status;
} CutFPResult;

StringView cut_fp_get_command(CutFlagParser *fp, int argc, char **argv);

CutFPResult cut_fp_parse(CutFlagParser *fp, int argc, char **argv, SVList *out);


#endif // CUT_H


/************************************************
 * Implementation
 ************************************************/

// #define CUT_IMPL
#ifdef CUT_IMPL

#include <assert.h>
#include <stddef.h>
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
#define fstat      _fstat

#define makedir(x) _mkdir(x)

#else

#include <unistd.h>
#include <limits.h>
#include <ctype.h>

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

char *sv_alloc_cstr(StringView s)
{
    char *str = malloc(s.len+1);
    memcpy(str, s.data, s.len);
    str[s.len] = '\0';
    return str;
}

// Returns if a and b are equal.
bool _sv_equal(StringView a, StringView b)
{
    if (a.len != b.len) return false;

    for (size_t i = 0; i < a.len; i++)
        if (a.data[i] != b.data[i]) return false;

    return true;
}

void str_append_char(String *s, char v)
{
    da_append(s, v);
    str_append_null(s);
}

void str_append_view(String *s, StringView v)
{
    da_appendn(s, v.data, v.len);
    str_append_null(s);
}

void str_append_str(String *s, const String *v)
{
    da_appendn(s, v->data, v->len);
    str_append_null(s);
}
void str_append_cstr(String *s, const char *v)
{
    da_appendn(s, v, strlen(v));
    str_append_null(s);
}

// Append a formatted string to the string.
void str_appendf(String *s, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    str_appendvf(s, fmt, args);
    va_end(args);
}

// Append a variadic formatted string to the string.
void str_appendvf(String *s, const char *fmt, va_list args)
{
    va_list copy;
    va_copy(copy, args);
    int size = vsnprintf(NULL, 0, fmt, copy);
    va_end(copy);

    if (size < 0) return;
    char buffer[size+1];
    vsnprintf(buffer, (size_t)size+1, fmt, args);
    str_append_cstr(s, buffer);
}

// Insert an element at index n.
void str_insert(String *s, char v, size_t n)
{
    assert(n <= s->len);
    da_grow(s);
    for (size_t i = s->len; i > n; i--)
        da_at(s, i) = da_at(s, i-1);

    da_at(s, n) = v;
    s->len++;
}

// Find the index of a character.
size_t sv_find(StringView s, char v)
{
    size_t out = SIZE_MAX;
    for (size_t i = 0; i < s.len; i++)
    {
        if (s.data[i] == v)
        {
            out = i;
            break;
        }
    }
    return out;
}

// Shift a string view by n. Mutates input.
StringView sv_shift(StringView *s, size_t n)
{
    StringView out = {s->data, n};
    s->data += n;
    s->len -= n;
    return out;
}

// Shift a string view until the next instance of delim. Mutates input.
StringView sv_split(StringView *s, char delim)
{
    StringView out = *s;

    size_t n = sv_find(*s, delim);
    if (n == SIZE_MAX) 
    {
        s->len = 0;
        return out;
    }

    sv_shift(s, n+1);
    out.len = n;
    return out;
}

// Trim whitespaces from the string view.
StringView sv_trim(StringView s)
{
    while (s.len > 0)
    {
        if (!isspace(*s.data))
            break;
        s.data++;
        s.len--;
    }

    while (s.len > 0)
    {
        if (!isspace(s.data[s.len-1]))
            break;
        s.len--;
    }

    return s;
}

// Return a slice of the string view.
StringView sv_slice_opt(StringView s, SVSLiceOpt opt)
{
    if (opt.to == SIZE_MAX) opt.to = s.len;
    assert(opt.to <= s.len);
    assert(opt.from < opt.to);

    s.data += opt.from;
    s.len = opt.to - opt.from;
    return s;
}

// Read a full line from the file stream.
void str_readline(String *s, FILE *stream)
{
    char chunk[256];
    for (;;)
    {
        if (!fgets(chunk, sizeof(chunk), stream))
            break;

        str_append(s, chunk);
        StringView sv = {chunk, strlen(chunk)};
        if (sv_find(sv, '\n') != SIZE_MAX) break;
        if (sv.len < sizeof(chunk)-1) break;
    }
}

bool str_readfile(String *s, FILE *file)
{
    struct stat st;
    long file_size = -1;

    if (fstat(fileno(file), &st) == 0 && S_ISREG(st.st_mode) && st.st_size > 0)
        file_size = (long)st.st_size;

    if (file_size > 0)
    {
        da_resize(s, s->len + (size_t)file_size + 1);
        size_t total = 0;
        while (total < (size_t)file_size)
        {
            size_t n = fread(s->data + s->len + total, 1,
                    (size_t)file_size-total, file);
            if (n == 0) break;
            total += n;
        }
        s->len += total;
        s->data[s->len] = '\0';
        return !ferror(file);
    }

    for (;;)
    {
        da_resize(s, s->len+1024+1);
        size_t n = fread(s->data + s->len, 1, 1024, file);
        s->len += n;
        if (n < 1024)
        {
            s->data[s->len] = '\0';
            return !ferror(file);
        }
    }
}

// Check if the string view starts with a prefix.
bool sv_startswith(StringView s, StringView prefix)
{
    if (prefix.len == 0) return true;
    if (s.len < prefix.len) return false;
    s = sv_slice(s, .to=prefix.len);
    return sv_equal(s, prefix);
}

// Check if the string view ends with a suffix.
bool sv_endswith(StringView s, StringView suffix)
{
    if (suffix.len == 0) return true;
    if (s.len < suffix.len) return false;
    s = sv_slice(s, .from=s.len-suffix.len);
    return sv_equal(s, suffix);
}

// Convert a string view to int.
bool sv_to_int(StringView s, int *out)
{
    if (s.len == 0) return false;

    bool neg = false;

    if (sv_startswith(s, SV("+")))
        sv_shift(&s, 1);

    if (sv_startswith(s, SV("-")))
    {
        sv_shift(&s, 1);
        neg = true;
    }

    unsigned int mag = 0;
    const unsigned int max_pos = INT_MAX;
    const unsigned int max_neg = INT_MAX + 1u;

    for (size_t i = 0; i < s.len; i++)
    {
        char c = s.data[i];
        if (c < '0' || c > '9') return false;

        unsigned int digit = c - '0';
        unsigned int limit = neg ? max_neg : max_pos;

        if (mag > (limit - digit) / 10)
            return false;

        mag = mag * 10 + digit;
    }

    *out = neg ? -mag : mag;
    return true;
}

void svlist_join(SVList *sv, String *sb, StringView delim)
{
    DA_FOR(sv, i)
    {
        str_appendf(sb, SV_FMT, SV_ARG(da_at(sv, i)));
        if (i < sv->len-1)
            str_appendf(sb, SV_FMT, SV_ARG(delim));
    }
}

bool svlist_contains(SVList *sl, StringView v)
{
    DA_FOR(sl, i)
        if (sv_equal(da_at(sl, i), v)) return true;

    return false;
}

// Format the string list into a whitespace-separated string.
static void command_format(SVList *sl, String *sb, StringView prefix)
{
    DA_FOR(sl, i)
    {
        str_appendf(sb, SV_FMT, SV_ARG(prefix));
        str_appendf(sb, SV_FMT" ", SV_ARG(da_at(sl, i)));
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
    str_append_view(sb, base);

#ifdef _WIN32
    str_appendf(sb, ".exe");
#endif
}

// Run a external command.
static void exec_command(StringView cmd)
{
    DEV_INFO(SV_FMT, SV_ARG(cmd));

    SV_TO_CSTR(cmd, cmd_buf);

    if (system(cmd_buf) != 0)
        DEV_FATAL("Failed to execute command.");
}

// Remove a path.
static void remove_path(StringView path)
{
    String cmd;
    str_init(&cmd);

#ifdef _WIN32
    str_appendf(&cmd, "del /q /s ");
#else
    str_appendf(&cmd, "rm -rf ");
#endif
    str_appendf(&cmd, "\""SV_FMT"\"", SV_ARG(path));
    exec_command(SV(cmd));
    str_free(&cmd);
}

static StringView get_base_name(StringView file)
{
    StringView base = sv_split(&file, '.');

    size_t next_fwd = sv_find(base, '/');
    char delim = next_fwd == SIZE_MAX ? '\\' : '/';

    for (;;)
    {
        StringView next = sv_split(&base, delim);
        if (base.len == 0)
            return next;
    }
}

/************************************************
 * Logging
 ************************************************/

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
    str_init(log.message);
    assert(log.message);
    str_appendvf(log.message, fmt, args);

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
        CutLog log = da_at(logs, ei);

        fprintf(fdout, "%s", prefix);

        if (is_ansi) fprintf(fdout, AFMT_DIM);
        fprintf(fdout, "["SV_FMT":%d] ", SV_ARG(log.file), log.line);
        if (is_ansi) fprintf(fdout, AFMT_RESET);

        switch (log.level)
        {
            case CUT_LOG_INFO: 
                fprintf(fdout, "[INFO] "); 
                break;

            case CUT_LOG_DEBUG: 
                if (is_ansi) fprintf(fdout, AFMT_RESET);
                fprintf(fdout, "[DEBUG] "); 
                break;

            case CUT_LOG_ERROR: 
                if (is_ansi) fprintf(fdout, ACOLOR_YELLOW);
                fprintf(fdout, "[ERROR] "); 
                break;

            case CUT_LOG_FATAL: 
                if (is_ansi) fprintf(fdout, ACOLOR_RED);
                fprintf(fdout, "[FATAL] "); 
                break;
        }
        fprintf(fdout, SV_FMT"\n", SV_ARG(*log.message));
        if (is_ansi) fprintf(fdout, AFMT_RESET);
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
    if (ansi) fprintf(fdout, AFMT_DIM);
    fprintf(fdout, "["SV_FMT":%d] ", SV_ARG(test->file), test->line);
    if (ansi) fprintf(fdout, AFMT_RESET);

    if (ansi) fprintf(fdout, ACOLOR_CYAN);
    fprintf(fdout, SV_FMT, SV_ARG(test->name));
    if (ansi) fprintf(fdout, AFMT_RESET);

    for (size_t i = test->name.len; i < cut_test_name_max; i++)
        fprintf(fdout, " ");

    fprintf(fdout, " ... ");
}

// Print the status of a test's logs and return if it failed.
static bool test_case_status_print(CutLogList *logs, FILE* fdout, bool ansi)
{
    size_t failure_count = 0;
    DA_FOR(logs, i)
    {
        CutLog log = da_at(logs, i);
        if (log.level == CUT_LOG_ERROR || log.level == CUT_LOG_FATAL)
            failure_count++;
    }

    if (failure_count > 0)
    {
        if (ansi) fprintf(fdout, ACOLOR_RED);
        fprintf(fdout, "failed\n");
        if (ansi) fprintf(fdout, AFMT_RESET);
        return true;
    }
    else
    {
        if (ansi) fprintf(fdout, ACOLOR_GREEN);
        fprintf(fdout, "passed\n");
        if (ansi) fprintf(fdout, AFMT_RESET);
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
    str_init(&exe);
    append_exe_name(&exe, exe_name);

    if (get_mtime(file, &mtime_file) != 0)
        return true;

    if (get_mtime(SV(exe), &mtime_exe) != 0)
        return true;

    return difftime(mtime_file, mtime_exe) > 0;
}

// Get the backup name for the script executable.
static void old_script_exe_name(String *sb)
{
    append_exe_name(sb, cut_builder.script_name);
    str_appendf(sb, ".old");
}

static void cmd_add_objs(CutUnit *unit, String *sb)
{
    DA_FOR(&unit->sources, i)
    {
        StringView src = da_at(&unit->sources, i);
        StringView base = get_base_name(src);
        str_appendf(sb, SV_FMT PATH_SEP, SV_ARG(cut_builder.build_dir));
        str_appendf(sb, SV_FMT".o ", SV_ARG(base));
    }
}

static void cmd_add_cc(String *sb)
{
    str_appendf(sb, SV_FMT" ", SV_ARG(cut_builder.cc));
}

static void cmd_add_srcs(CutUnit *unit, String *sb)
{
    command_format(&unit->sources, sb, SV(""));
}

static void cmd_add_cflags(CutUnit *unit, String *sb)
{
    command_format(&unit->includes, sb, SV("-I"));
    command_format(&unit->flags, sb, SV(""));
    command_format(&unit->defines, sb, SV("-D"));
}

static void cmd_add_links(CutUnit *unit, String *sb)
{
    command_format(&unit->lib_dirs, sb, SV("-L"));
    command_format(&unit->libs, sb, SV("-l"));
}

// Generate the build command for a unit.
static void cmd_build_exe(CutUnit *unit, String *sb)
{
    cmd_add_cc(sb);
    cmd_add_srcs(unit, sb);
    cmd_add_cflags(unit, sb);
    cmd_add_links(unit, sb);
    str_appendf(sb, "-o "SV_FMT PATH_SEP SV_FMT" ", 
            SV_ARG(cut_builder.build_dir), SV_ARG(unit->name));
}

static void cmd_build_obj(CutUnit *unit, StringView src, bool pic, String *sb)
{
    cmd_add_cc(sb);
    str_append(sb, "-c ");
    if (pic) str_append(sb, "-fPIC ");

    str_appendf(sb, SV_FMT" ", SV_ARG(src));

    cmd_add_cflags(unit, sb);

    StringView base = get_base_name(src);
    str_appendf(sb, "-o "SV_FMT PATH_SEP SV_FMT".o ", 
            SV_ARG(cut_builder.build_dir), SV_ARG(base));

    exec_command(SV(sb));
}

static void cmd_build_shared(CutUnit *unit, String *sb)
{
    cmd_add_cc(sb);
    str_append(sb, "-shared ");
    cmd_add_objs(unit, sb);
    StringView libname = unit->libname.len == 0 ? unit->name : unit->libname;
    str_appendf(sb, "-o "SV_FMT PATH_SEP, SV_ARG(cut_builder.lib_dir));
    str_appendf(sb, "lib"SV_FMT".so ", SV_ARG(libname));
}

static void cmd_build_static(CutUnit *unit, String *sb)
{
    str_append(sb, "ar rcs ");
    str_appendf(sb, SV_FMT PATH_SEP, SV_ARG(cut_builder.lib_dir));
    StringView libname = unit->libname.len == 0 ? unit->name : unit->libname;
    str_appendf(sb, "lib"SV_FMT".a ", SV_ARG(libname));
    cmd_add_objs(unit, sb);
}

// Generate the command to run an executable. Contains a trailing whitespace.
static void cmd_run_exe(StringView name, StringView parent, String *sb)
{
    str_appendf(sb, "."PATH_SEP);
    if (parent.len > 0)
        str_appendf(sb, SV_FMT PATH_SEP, SV_ARG(cut_builder.build_dir));

    append_exe_name(sb, name);
    str_appendf(sb, " ");
}

// Rebuild the build script.
static void cut_rebuild(size_t argc, StringView *argv)
{
    String sb;
    str_init(&sb);
    append_exe_name(&sb, cut_builder.script_name);

    String new_path;
    str_init(&new_path);
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

    str_free(&new_path);

    str_reset(&sb);

    str_appendf(&sb, SV_FMT" ",    SV_ARG(cut_builder.cc));
    str_appendf(&sb, SV_FMT" ",    SV_ARG(cut_builder.file));
    str_appendf(&sb, "-o "SV_FMT,  SV_ARG(cut_builder.script_name));
    exec_command(SV(sb));

    str_reset(&sb);
    cmd_run_exe(cut_builder.script_name, SV(""), &sb);
    for (size_t i = 1; i < argc; i++)
        str_appendf(&sb, SV_FMT" ", SV_ARG(argv[i]));

    exec_command(SV(sb));

    exit(0);
}

void cut_create_dir(StringView dir)
{
    switch (mkdir_if_not_exist(dir))
    {
        case MKDIR_CREATED:
            DEV_INFO("Created directory: "SV_FMT, SV_ARG(dir));
            break;

        case MKDIR_EXISTS:
            break;

        case MKDIR_FAILED:
            DEV_FATAL("Unknown error occured during directory creation.");
    }
}

// Initialize the build with options.
void cut_build_init_opt(StringView file, CutBuilderOpt opt)
{
    cut_builder.cc = opt.cc;
    cut_builder.build_dir = opt.build_dir;
    cut_builder.lib_dir = opt.lib_dir;
    cut_builder.script_name = opt.script_name;
    cut_builder.file = file;

    cut_create_dir(opt.build_dir);
    cut_create_dir(opt.lib_dir);
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

static void cut_build_exe(CutUnit *exe, bool run, StringView *args, int count)
{
    String cmd;
    str_init(&cmd);

    cmd_build_exe(exe, &cmd);
    exec_command(SV(cmd));

    if (run)
    {
        str_reset(&cmd);
        cmd_run_exe(exe->name, cut_builder.build_dir, &cmd);
        for (int i = 0; i < count; i++)
            str_appendf(&cmd, "\""SV_FMT"\" ", SV_ARG(args[i]));

        exec_command(SV(cmd));
    }

    str_free(&cmd);
}

static void cut_build_objs(CutUnit *unit)
{
    String cmd;
    str_init(&cmd);

    bool shared = unit->kind == CUT_UNIT_LIB_SHARED;
    DA_FOR(&unit->sources, i)
    {
        StringView src = da_at(&unit->sources, i);
        cmd_build_obj(unit, src, shared, &cmd);
        exec_command(SV(cmd));
        str_reset(&cmd);
    }
    str_free(&cmd);
}

static void cut_build_lib(CutUnit *lib)
{
    String cmd;
    str_init(&cmd);

    cut_build_objs(lib);

    if (lib->kind == CUT_UNIT_LIB_SHARED)
        cmd_build_shared(lib, &cmd);
    else
        cmd_build_static(lib, &cmd);
    exec_command(SV(cmd));

    str_free(&cmd);
}

static void cut_build_clean(void)
{
    String cmd;
    str_init(&cmd);

    str_appendf(&cmd, SV_FMT PATH_SEP "*", SV_ARG(cut_builder.build_dir));
    remove_path(SV(cmd));

    str_reset(&cmd);
    old_script_exe_name(&cmd);
    remove_path(SV(cmd));

    str_free(&cmd);
}

// Run build.
int cut_build_run(int argc, char **argv)
{
    StringView args[argc];
    for (int i = 0; i < argc; i++)
        args[i] = SV(argv[i]);

    if (should_rebuild(cut_builder.file, cut_builder.script_name))
        cut_rebuild(argc, args);

    StringView subcmd = args[1];

    if (argc == 2 && sv_equal(subcmd, "clean"))
    {
        cut_build_clean();
    }
    else if (argc >= 3 && sv_equal(subcmd, "rebuild"))
    {
        cut_rebuild(argc-1, args+1);
    }
    else if (argc >= 3)
    {
        bool build = sv_equal(subcmd, "build");
        bool run = sv_equal(subcmd, "run");

        if (build || run)
        {
            CutUnit *unit = NULL;
            unit = cut_build_find_unit(args[2]);
            if (!unit) 
                DEV_FATAL("Unit '"SV_FMT"' does not exist.", SV_ARG(args[2]));

            switch (unit->kind)
            {
                case CUT_UNIT_EXE:
                    cut_build_exe(unit, run, args+3, argc-3);
                    break;

                case CUT_UNIT_LIB_STATIC:
                case CUT_UNIT_LIB_SHARED:
                    cut_build_lib(unit);
                    break;
            }
        }
    }
    else
    {
        printf("Usage: \n"
                "    <cut> help             show this help\n"
                "    <cut> build <name>     build the unit\n"
                "    <cut> run <name>       build and run the unit\n"
                "    <cut> clean            clean artifacts\n"
                "    <cut> rebuild <cmd>    rebuild script executable\n");

        return 1;
    }

    return 0;
}


/************************************************
 * CLI Flag Parsing
 ************************************************/

// Initialize a flag parser.
void cut_fp_init(CutFlagParser *fp)
{
    da_init(&fp->commands);
    da_init(&fp->optional);
}

// Reset a flag parser's configuration.
void cut_fp_reset(CutFlagParser *fp)
{
    da_reset(&fp->commands);
    da_reset(&fp->optional);
}

// Free a flag parser.
void cut_fp_free(CutFlagParser *fp)
{
    da_free(&fp->commands);
    da_free(&fp->optional);
}

// Add a command to the flag parser.
void cut_fp_add_command(CutFlagParser *fp, StringView cmd)
{
    da_append(&fp->commands, cmd);
}

// Add an optional flag to the flag parser.
void cut_fp_add_flag_opt(CutFlagParser *fp, CutFlagKind kind, 
        void *data, StringView name, CutFlagOpt opt)
{
    CutFlag f = {
        .data = data,
        .long_name = name,
        .kind = kind,
        .short_name = opt.short_name,
        .desc = opt.desc,
    };
    da_append(&fp->optional, f);
}

static bool flag_find_by_name(CutFlagParser *fp, StringView name, CutFlag *out)
{
    DA_FOR(&fp->optional, i)
    {
        CutFlag f = da_at(&fp->optional, i);
        if (sv_equal(f.long_name, name))
        {
            if (out)
                *out = f;
            return true;
        }
    }
    return false;
}

static bool flag_find_by_short(CutFlagParser *fp, char s_name, CutFlag *out)
{
    DA_FOR(&fp->optional, i)
    {
        CutFlag f = da_at(&fp->optional, i);
        if (f.short_name == s_name)
        {
            if (out)
                *out = f;
            return true;
        }
    }
    return false;
}

typedef enum
{
    FLAG_NONE,
    FLAG_SEP,
    FLAG_LONG,
    FLAG_SHORT,
} FlagShape;

static FlagShape flag_shape(StringView s)
{
    if (sv_startswith(s, SV("--")))
    {
        return s.len == 2 ? FLAG_SEP : FLAG_LONG;
    }
    if (sv_startswith(s, SV("-")))
    {
        return FLAG_SHORT;
    }
    return FLAG_NONE;
}

static CutFPResult fp_ok(void)
{
    return (CutFPResult){.msg=NULL, .status=CUT_FP_OK};
}

static CutFPResult fp_error(CutFPStatus s, CutFlag f, StringView v)
{
    CutFPResult r = {0};
    r.status = s;
    r.msg = malloc(sizeof(String));
    str_init(r.msg);

    switch (s)
    {
        case CUT_FP_MISSING_VALUE:
            str_appendf(r.msg, "missing value for flag '"SV_FMT"'", SV_ARG(f.long_name));
            break;

        case CUT_FP_INVALID_VALUE:
            str_appendf(r.msg, "invalid value for flag '"SV_FMT"': "SV_FMT, 
                    SV_ARG(f.long_name), SV_ARG(v));
            break;

        default:
            break;
    }
    return r;
}

StringView cut_fp_get_command(CutFlagParser *fp, int argc, char **argv)
{
    if (argc <= 1) return SV("");

    StringView arg = SV(argv[1]);

    DA_FOR(&fp->commands, i)
    {
        StringView cmd = da_at(&fp->commands, i);
        if (sv_equal(arg, cmd))
            return cmd;
    }
    return SV("");
}

CutFPResult cut_fp_parse(CutFlagParser *fp, int argc, char **argv, SVList *out)
{
    if (argc <= 1) return fp_ok();

    bool seen_sep = false;

    int pos = 1;

    if (cut_fp_get_command(fp, argc, argv).len > 0)
        pos++;

    while (pos < argc)
    {
        bool is_short = false;

        if (seen_sep)
            goto positional;

        CutFlag flag = {0};
        StringView val_str = {0};
        StringView arg = SV(argv[pos]);

        switch (flag_shape(arg))
        {
            case FLAG_SEP:
                seen_sep = true;
                pos++;
                // fallthrough
            case FLAG_NONE:
                goto positional;

            case FLAG_SHORT:
                sv_shift(&arg, 1);

                if (arg.len > 1)
                {
                    for (size_t i = 0; i < arg.len; i++)
                    {
                        CutFlag f = {0};
                        if (!flag_find_by_short(fp, arg.data[i], &f))
                            goto positional;

                        switch (f.kind)
                        {
                            case CUT_FLAG_BOOL:
                                *(bool *)f.data = true;
                                break;

                            case CUT_FLAG_INT:
                                *(int *)f.data += 1;
                                break;

                            default:
                                goto positional;
                        }
                    }
                    pos++;
                    continue;
                }

                if (!flag_find_by_short(fp, arg.data[0], &flag))
                    goto positional;

                is_short = true;
                break;

            case FLAG_LONG:
                sv_shift(&arg, 2);
                StringView name = sv_split(&arg, '=');

                if (!flag_find_by_name(fp, name, &flag))
                    goto positional;

                if (arg.len > 0)
                    val_str = arg;

                break;
        }

        if (flag.kind == CUT_FLAG_BOOL)
        {
            *(bool *)flag.data = true;
            pos++;
            continue;
        }

        if (val_str.len == 0 && pos+1 < argc)
        {
            val_str = SV(argv[pos+1]);
            switch (flag_shape(val_str))
            {
                case FLAG_SHORT:
                    if (flag_find_by_short(fp, val_str.data[1], NULL))
                    {
                        if (flag.kind == CUT_FLAG_INT && is_short)
                            break;
                        return fp_error(CUT_FP_MISSING_VALUE, flag, val_str);
                    }
                    break;

                case FLAG_SEP:
                    return fp_error(CUT_FP_INVALID_VALUE, flag, val_str);

                case FLAG_LONG:
                    if (flag_find_by_name(fp, sv_slice(val_str, .from=2), NULL))
                        return fp_error(CUT_FP_MISSING_VALUE, flag, val_str);
                    break;

                case FLAG_NONE:
                    break;
            }
        }

        if (flag.kind == CUT_FLAG_STR)
        {
            if (val_str.len == 0)
                return fp_error(CUT_FP_MISSING_VALUE, flag, val_str);
            *(StringView *)flag.data = val_str;
            pos++;
        }

        if (flag.kind == CUT_FLAG_INT)
        {
            int val = 0;
            if (sv_to_int(val_str, &val))
            {
                *(int *)flag.data = val;
                pos++;
            }
            else if (is_short)
            {
                *(int *)flag.data += 1;
            }
            else
            {
                return fp_error(CUT_FP_INVALID_VALUE, flag, val_str);
            }
        }

        pos++;
        continue;

positional:
        da_append(out, SV(argv[pos]));
        pos++;
    }

    return fp_ok();
}

#endif // CUT_IMPL
