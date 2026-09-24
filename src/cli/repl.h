#ifndef REPL_H
#define REPL_H

#include "cut.h"
#include "ccal/ccal.h"

#define DESC_FORMAT    "set the display format for numeric values"
#define DESC_AUTO      "fixed-point by default, scientific for large exponent"
#define DESC_FIXED     "force fixed-point notation"
#define DESC_SCI       "force scientific notation"

#define DESC_RATIONAL  "display rational form alongside output for exact values"
#define DESC_OBASE     "set the output base"
#define DESC_IBASE     "set the default input base"
#define DESC_PRECISION "set the precision for real value"
#define DESC_TRUNCATE  "set the max decimal places to truncate at"

#define FORMAT_LIST \
                    "Display Formats:\n" \
                    "    auto                        "DESC_AUTO     "\n" \
                    "    fixed                       "DESC_FIXED    "\n" \
                    "    sci | scientific            "DESC_SCI      "\n"

#define OPTIONS_REPL \
                    "Options:\n" \
                    "    ob  | obase     <n>         "DESC_OBASE    "\n" \
                    "    ib  | ibase     <n>         "DESC_IBASE    "\n" \
                    "    pr  | precision <n>         "DESC_PRECISION"\n" \
                    "    tr  | truncate  <n>         "DESC_TRUNCATE "\n" \
                    "    fmt | format    <fmt>       "DESC_FORMAT   "\n" \
                    "    rat | rational              "DESC_RATIONAL "\n"

extern const char cli_help[];
extern const char repl_help[];

void repl_start(const CCalCtx *ctx);
bool run_script(StringView path, const CCalCtx *ctx);
bool run_eval(FILE *fdout, StringView input, const CCalCtx *ctx);

#endif
