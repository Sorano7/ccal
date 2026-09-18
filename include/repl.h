#ifndef REPL_H
#define REPL_H

#include "vm.h"

#define FORMAT    "set the display format for numeric values"
#define AUTO      "fixed-point by default, scientific for large exponent"
#define FIXED     "force fixed-point notation"
#define SCI       "force scientific notation"

#define RATIONAL  "display rational form alongside output for exact values"
#define OBASE     "set the output base"
#define IBASE     "set the default input base"
#define PRECISION "set the precision for real value"
#define TRUNCATE  "set the max decimal places to truncate at"

#define FORMAT_LIST \
                    "Display Formats:\n" \
                    "    auto                        "AUTO     "\n" \
                    "    fixed                       "FIXED    "\n" \
                    "    sci | scientific            "SCI      "\n"

#define OPTIONS_REPL \
                    "Options:\n" \
                    "    ob  | obase     <n>         "OBASE    "\n" \
                    "    ib  | ibase     <n>         "IBASE    "\n" \
                    "    pr  | precision <n>         "PRECISION"\n" \
                    "    tr  | truncate  <n>         "TRUNCATE "\n" \
                    "    fmt | format    <fmt>       "FORMAT   "\n" \
                    "    rat | rational              "RATIONAL "\n"

extern const char cli_help[];
extern const char repl_help[];

void repl_start(VM *vm, RenderCtx *ctx);
bool run_script(VM *vm, StringView path, RenderCtx *ctx);
bool run_eval(VM *vm, FILE *fdout, StringView input, RenderCtx *ctx);

#endif
