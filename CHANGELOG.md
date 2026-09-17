# Change Log
All notable changes to this project will be documented in this file.
 
The format is based on [Keep a Changelog](http://keepachangelog.com/) and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]

### Added
- Comments with `--`. Subtraction operator now must be space-separated from negation.
- Multi-line support for REPL when the input ends with `;`.
- REPL command to clear the session.
- CLI subcommand to run a script.

### Changed
- Lambda literal now must be inside parenthesis unless on the right of assignment or is lambda body.
- Conditional now has shape `<if> ? <then> : <else>`.

### Fixed 
- Lambda no longer suppress undefined symbol error.
- Self-referential lambdas no longer leaks memory.
- Diagnostics now display against the correct source line.


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
