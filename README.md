# ccal
An arbitrary-precision calculator written in C.

## Quick Start

Building from source:

```sh
cc cut.c -o cut && ./cut build ccal
```

## Usage

```
ccal help                --  show help
ccal <opts>              --  start REPL
ccal run <expr> <opts>   --  run oneshot
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

Basic shape of a number:

```
<base#?><I><.N?>(R?)
```

There are two ways to spell a sequence of digits.

Alphanumerics: `[0-9][A-Z][a-z]_`. Up to base 62, and case-insensitive until base 36.

Examples:

```
12#1A3
16#ff
1_024
```

Digit list: `[..., ..., ...]`. Up to base-`2^32 - 1` or `2^64 - 1` depending on the platform.

```
12#[1, 10, 3]
16#[15, 15]
[1, 0, 2, 4]
```

The base annotation scopes to a single expression.

```
> 16#FF
255

> 16#FF + 100
355

> 16#(FF + 100)
511
```

## Variables

Variables are prefixed with `@` to distinguish from digits. Assignment is an expression.

```
> @foo = 42
42

> @foo
42
```

Builtin symbols include:

```
@true
@false
@@       --  last value in history
```
