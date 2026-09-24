#define CUT_IMPL
#include "../shared/cut.h"

void core_config(CutUnit *u)
{
    cut_unit_sources(u, "src/ccal.c");
    cut_unit_sources(u, "src/ast.c");
    cut_unit_sources(u, "src/creal.c");
    cut_unit_sources(u, "src/number.c");
    cut_unit_sources(u, "src/lexer.c");
    cut_unit_sources(u, "src/parser.c");
    cut_unit_sources(u, "src/value.c");
    cut_unit_sources(u, "src/vm.c");

    cut_unit_includes(u, "../shared", "include", "src/include");
    cut_unit_flags(u, "-g", "-Wall", "-Wextra", "-Wno-override-init");

    cut_unit_flags(u, "-static");
    cut_unit_libs(u, "mpfi", "mpfr", "gmp", "m");
}

void test_config(CutUnit *u)
{
    cut_unit_sources(u, "tests/main.c");
    cut_unit_sources(u, "tests/lexer.c");
    cut_unit_sources(u, "tests/parser.c");
    cut_unit_sources(u, "tests/creal.c");
    cut_unit_sources(u, "tests/vm.c");
    cut_unit_sources(u, "tests/api.c");

    core_config(u);
}

int main(int argc, char **argv)
{
    cut_build_init();

    CutUnit lib;
    cut_unit_init(&lib, "lib", CUT_UNIT_LIB_STATIC);
    cut_unit_lib_name(&lib, "ccal");
    core_config(&lib);

    CutUnit test;
    cut_unit_init(&test, "test", CUT_UNIT_EXE);
    test_config(&test);

    cut_build_add(&lib, &test);
    return cut_build_run(argc, argv);
}
