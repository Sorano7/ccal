#define CUT_IMPL
#include "src/include/cut.h"

void core_config(CutUnit *u)
{
    cut_unit_sources(u, "src/core/ast.c");
    cut_unit_sources(u, "src/core/creal.c");
    cut_unit_sources(u, "src/core/number.c");
    cut_unit_sources(u, "src/core/lexer.c");
    cut_unit_sources(u, "src/core/parser.c");
    cut_unit_sources(u, "src/core/value.c");
    cut_unit_sources(u, "src/core/vm.c");

    cut_unit_includes(u, "src/include");
    cut_unit_flags(u, "-g", "-Wall", "-Wextra", "-Wno-override-init");
    // cut_unit_flags(u, "-fsanitize=address,undefined");
}

void add_links(CutUnit *u)
{
    cut_unit_flags(u, "-static");
    cut_unit_libs(u, "mpfi", "mpfr", "gmp", "m");
}

void lib_config(CutUnit *u)
{
    cut_unit_lib_name(u, "ccal");
    cut_unit_sources(u, "src/api/ccal.c");
    cut_unit_includes(u, "include");
    core_config(u);
}

void cli_config(CutUnit *u)
{
    cut_unit_sources(u, "src/cli/main.c");
    cut_unit_sources(u, "src/cli/repl.c");
    cut_unit_libs(u, "readline", "ncursesw");

    add_links(u);
    lib_config(u);
}

void test_config(CutUnit *u)
{
    cut_unit_sources(u, "tests/main.c");
    cut_unit_sources(u, "tests/creal.c");
    cut_unit_sources(u, "tests/lexer.c");
    cut_unit_sources(u, "tests/parser.c");
    cut_unit_sources(u, "tests/vm.c");
    cut_unit_sources(u, "tests/api.c");

    add_links(u);
    lib_config(u);
}


int main(int argc, char **argv)
{
    cut_build_init();

    CutUnit lib;
    cut_unit_init(&lib, "lib", CUT_UNIT_LIB_STATIC);
    lib_config(&lib);

    CutUnit cli;
    cut_unit_init(&cli, "ccal", CUT_UNIT_EXE);
    cli_config(&cli);

    CutUnit test;
    cut_unit_init(&test, "test", CUT_UNIT_EXE);
    test_config(&test);

    cut_build_add(&lib, &cli, &test);
    return cut_build_run(argc, argv);
}
