# ccal
An arbitrary-precision calculator written in C.

## Quick Start

Building from source:

```sh
cc cut.c -o cut && ./cut build ccal
```

Run `ccal` to start the REPL, `ccal run <expr>` to evaluate an expression.


## Syntax

### Numbers

Basic shape of a number:

```
<base#?><I><.N?>(R?)
```

There are two ways to spell a sequence of digits.

Alphanumerics: `[0-9][A-Z][a-z]_`. Up to base 62.

Examples:

```
12#1A3
16#FF
1024
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
