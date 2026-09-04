# ccal
An arbitrary-precision calculator written in C.

## Quick Start

Building from source:

```sh
cc cut.c -o cut && ./cut build ccal
```

## Usage

```
ccal help                 --  show help
ccal <opts>               --  start interactive REPL
ccal eval <expr> <opts>   --  evaluate an expression
```

Options include:

```
-d | --decimal
-r | --rational
-o | --obase    <n>
-i | --ibase    <n>
-t | --truncate <n>
```

Similar commands are available in REPL, prefixed with `:`.

## Syntax

### Numbers

```
base#I.N(R)
```

There are two ways to spell a sequence of digits, and a number must only contain one spelling.

1. Alphanumerics: `[0-9][A-Z][a-z]_`. 
    - Up to base 62, and case-insensitive until base 36.
2. Digit list: `[..., ..., ...]`. 
    - Up to base-`2^32 - 1` or `2^64 - 1` depending on the platform.

Examples:

```
12#1A3    == 12#[1, 10, 3]   == 267
16#a.a    == 16#[10].[10]    == 10.625
1_000.(3) == [1,0,0,0].([3]) == 3001/3
```

The base annotation scopes to a single expression.

```
16#FF         -- 255
16#FF + 100   -- 355
16#(FF + 100) -- 511
```

## Variables

Variables are prefixed with `\` to distinguish from digits.

```
\foo = 42
\x = \y = \foo * 2
\true, \false, \ans, ...
```

## Lambdas

Defined as `<param> : <body>`.

```
\x: \x + 1       -- fn (x) x + 1
\x: \y: \x \y    -- fn (x) fn (y) x + y
```

Two ways of application:
1. White space (left-associative): `\f \x`.
2. Dollar sign (right-associative): `\f $ \g \x`.
