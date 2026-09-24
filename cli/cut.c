#define CUT_IMPL
#include "../shared/cut.h"

void config(CutUnit *u)
{
    cut_unit_sources(u, "src/main.c");
    cut_unit_sources(u, "src/repl.c");

    cut_unit_includes(u, "../shared", "../core/include", "include");
    cut_unit_flags(u, "-g", "-Wall", "-Wextra", "-Wno-override-init");

    cut_unit_lib_dirs(u, "../core/lib");
    cut_unit_libs(u, "ccal", "mpfr", "mpfi", "gmp", "m");
    cut_unit_libs(u, "readline", "ncursesw");
}

int main(int argc, char **argv)
{
    cut_build_init();

    CutUnit cli;
    cut_unit_init(&cli, "ccal", CUT_UNIT_EXE);
    config(&cli);

    cut_build_add(&cli);
    return cut_build_run(argc, argv);
}
