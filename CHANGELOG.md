# Change Log
All notable changes to this project will be documented in this file.
 
The format is based on [Keep a Changelog](http://keepachangelog.com/) and this project adheres to [Semantic Versioning](http://semver.org/).

## [Unreleased]

### Added
- `^` uses real path for non-integer and large exponents.
- Error handling for builtin real functions.

### Fixed
- Display interval form when integer part diverges instead of truncating.
- Prevent a crash/hang when computing/rendering astronomical values from `^` or `'pow`.

## [1.0.0] - 2026-09-14

### Added
- Initial release.
- Arbitrary precision exact and real arithmetics.
- Arbitrary input base syntax and configurable output base.
- Variable assignment.
- Lambda-calculus-styled functions with recursion.
- Built-in math functions.
- Options as CLI flags and REPL commands.
