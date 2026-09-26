#define CUT_IMPL
#include "../shared/cut.h"

void config(CutUnit *u)
{
    cut_unit_sources(u, "src/main.c");
    cut_unit_sources(u, "src/repl.c");

    cut_unit_includes(u, "../shared", "../core/include", "include");
    cut_unit_flags(u, "-g", "-Wall", "-Wextra", "-Wno-override-init");

    cut_init_static_link(u, true);
    cut_unit_lib_dirs(u, "../core/lib");
    cut_unit_libs(u, "ccal", "mpfi", "mpfr", "gmp", "m");
    cut_unit_libs(u, "readline", "ncursesw", "tinfow");
}

int main(int argc, char **argv)
{
    cut_build_init();

    CutUnit cli;
    cut_unit_init(&cli, "cli", CUT_UNIT_EXE);
    cut_unit_out_name(&cli, "ccal");
    config(&cli);

    cut_build_add(&cli);
    return cut_build_run(argc, argv);
}
