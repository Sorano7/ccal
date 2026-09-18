# ccal
An arbitrary-precision calculator written in C.

## Quick Start

```
ccal help                 --  show help
ccal <opts>               --  start interactive REPL
ccal run  <opts> <path>   --  run a script
ccal eval <opts> <expr>   --  evaluate an expression
```

Options include:

```
-o | --obase     <n>      -- set output base
-i | --ibase     <n>      -- set input base
-t | --truncate  <n>      -- max decimal places
-p | --precision <n>      -- precision for real values
-f | --format    <fmt>    -- display format for real values
-r | --rational           -- show rational form for exact values
```

Similar commands are available in REPL, prefixed with `:`.


## Basic Syntax

Expression-only. Certain expressions may span multiple lines, and multiple expressions can be on the same line separated with `;`. 

Use `--` for comments, which ignores until a newline or another `--`.

There are three types: `exact`, `real`, and `lambda`.

## Exact Numbers

A number literal is written as `I.N(R)`. Omitting the leading zero when `I` is zero is not supported.

There are two ways to spell a sequence of digits, and a literal must use one or the other throughout.

**Alphanumerics**

- Digits: `[0-9][A-Z][a-z]`.
- `_` can be used as separator and is ignored.
- Supports up to base 62, case-insensitive until base 36.
- Examples: `1_000_000`, `ff`, `0.hello(world)`.

**Digit List**

- A list of numeric digits surrounded by `[]` and separated by `,`.
- Supports base up to `2^32-1` or `2^64-1`.
- Examples: `[1, 0, 2, 4]`, `[15, 15]`, `[0].[1, 2]([3, 4])`.


### Base

Use the base tag `<base>#` in front of a number or a grouped expression to denote the base. The global `ibase` will be used for untagged literals.

Alternatively, prefixes such as `0x` and `0b` can be used for a single number literal.

```
16#ff           -- 255
16#(ff + 10)    -- 255 + 16
16#(ff + 0o10)  -- 255 + 8
```

## Variables

Variables are dynamically typed and prefixed with `'` to distinguish from digits, with the exception of when it is used as an infix lambda.

Assign to a variable with `=`. Assignment is an expression and right-associative.

```
'x = 100
'y = 'z = 'x
```

Use `'ans` to refer to the last value produced.

```
1 + 2; 'ans * 3    -- 9
```

## Lambdas

A lambda literal is `<param>: <body>`, with a single parameter and a single expression as body. Must be wrapped in brackets unless on the right of an assignment or as the body of another lambda.

Lambdas are pure and cannot mutate outer variables.

```
('x: 'x + 1)             -- (x) => x + 1
'add = 'x: 'y: 'x + 'y   -- (x) => (y) => x + y 
```

Application can be white-space (left-associative), `$` (right-associative), or ``<a> `<f>` <b>`` (infix).

```
'add 1 2
'map ('x: 'x ^ 'x) $ 1 `cons` 2 `cons` 'nil
```

Builtin lambdas include `'true`, `'false`, and math operations such as `'ln` and `'sqrt`.

Boolean builtins can be produced by comparison and equality operations such as `'a == 'b`. Note that these are evaluated eagerly.


### Conditionals and Recursion

Conditional has the shape `<if> ? <then> : <else>`, which is evaluated lazily. Named recursion works by referencing the lambda's binding name.

```
'fac = 'n: ('n == 0) ? 1 : 'n * 'fac ('n - 1)

'foldr = 'f: 'z: 'l: 'l 'z ('x: 'xs: 'f 'x $ 'foldr 'f 'z 'xs)
```

Guards can be used as syntactic sugar of conditionals, for lambda expressions. The else branch `'_` is required.

```
'fac = 'n:
       | 'n == 0 -> 1
       | '_      -> 'n * 'fac ('n - 1)
```

## Real Numbers

Real values are produced by operations that largely produce irrational values, such as `'sqrt`, `^` with non-integer exponent, etc. Exact values are lifted to real when real values are involved.

Real values are rendered with `~=` to distinguish from exact values, which is the same operator used for approximation.
