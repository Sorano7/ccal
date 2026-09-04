#define CUT_IMPL
#include "include/cut.h"

void shared_config(CutUnit *u)
{
    cut_unit_sources(u, "src/number.c");
    cut_unit_sources(u, "src/parser.c");
    cut_unit_sources(u, "src/ast.c");
    cut_unit_sources(u, "src/vm.c");

    cut_unit_includes(u, "include");
    cut_unit_flags(u, "-g", "-Wall", "-Wextra", "-Wno-override-init");
    cut_unit_libs(u, "gmp");
}

int main(int argc, char **argv)
{
    cut_build_init();

    CutUnit app;
    cut_unit_init(&app, "ccal", CUT_UNIT_EXE);
    cut_unit_sources(&app, "src/main.c");
    cut_unit_libs(&app, "readline", "ncursesw");
    cut_unit_flags(&app, "-static", "-O2");
    shared_config(&app);

    CutUnit test;
    cut_unit_init(&test, "test", CUT_UNIT_EXE);
    cut_unit_sources(&test, "tests/main.c");
    cut_unit_sources(&test, "tests/parser.c", "tests/vm.c");
    cut_unit_flags(&test, "-g");
    shared_config(&test);

    cut_build_add(&app, &test);

    return cut_build_run(argc, argv);
}
