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

void link_config(CutUnit *u)
{
    cut_unit_flags(u, "-static");
    cut_unit_libs(u, "mpfi", "mpfr", "gmp", "m");
}

void test_config(CutUnit *u)
{
    cut_unit_init(u, "test", CUT_UNIT_EXE);
    cut_unit_sources(u, "tests/main.c");
    cut_unit_sources(u, "tests/creal.c");
    cut_unit_sources(u, "tests/lexer.c");
    cut_unit_sources(u, "tests/parser.c");
    cut_unit_sources(u, "tests/vm.c");
    core_config(u);
    link_config(u);
}

void cli_config(CutUnit *u)
{

    cut_unit_init(u, "cli", CUT_UNIT_EXE);
    cut_unit_sources(u, "src/cli/main.c");
    cut_unit_sources(u, "src/cli/repl.c");
    cut_unit_libs(u, "readline", "ncursesw");
    core_config(u);
    link_config(u);
}

void lib_config(CutUnit *u)
{
    cut_unit_init(u, "lib", CUT_UNIT_LIB_STATIC);
    cut_unit_lib_name(u, "ccal");
    cut_unit_sources(u, "src/api/ccal.c");
    cut_unit_includes(u, "include");
    core_config(u);
}

int main(int argc, char **argv)
{
    cut_build_init();

    CutUnit cli;
    cli_config(&cli);

    CutUnit test;
    test_config(&test);

    CutUnit lib;
    lib_config(&lib);

    cut_build_add(&cli, &lib, &test);
    return cut_build_run(argc, argv);
}
