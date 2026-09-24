# Change Log
All notable changes to this project will be documented in this file.
 
The format is based on [Keep a Changelog](http://keepachangelog.com/) and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]

### Added
- Public interpreter API.
- Register native functions via API.

### Fixed
- No longer crashes on tab-completion after the session is cleared.
- No longer crashes when leading zero prefix is not followed by digits (e.g., `0x`).
- Native lambdas no longer incorrectly mutate state across calls.


## [2.0.0] - 2026-09-20

### Added
- Comments with `--`. Subtraction operator now must be space-separated from negation.
- Multi-line support for REPL.
- REPL command to clear the session.
- REPL tab-completion for commands and symbols.
- CLI subcommand to run a script.
- Modulo operator and builtin lambda.
- Guard expression with `|`.

### Changed
- Lambda literal now must be inside parenthesis unless on the right of assignment or is lambda body.
- Conditional now has shape `<if> ? <then> : <else>`.
- Some expressions can now span multiple lines
- Lowered the precedence of `$`.

### Fixed 
- Lambda no longer suppress undefined symbol error.
- Self-referential lambdas no longer leaks memory.
- Diagnostics now display against the correct source line and multi-line aware.


## [1.1.0] - 2026-09-15

### Added
- `^` uses real path for non-integer and large exponents.
- Error handling for builtin real functions.

### Fixed
- Display interval form when integer part diverges instead of truncating.
- Prevent a crash/hang when computing/rendering astronomical values from `^` or `'pow`.

### Changed
- No longer render zero with sign.


## [1.0.0] - 2026-09-14

### Added
- Initial release.
- Arbitrary precision exact and real arithmetics.
- Arbitrary input base syntax and configurable output base.
- Variable assignment.
- Lambda-calculus-styled functions with recursion.
- Built-in math functions.
- Options as CLI flags and REPL commands.
