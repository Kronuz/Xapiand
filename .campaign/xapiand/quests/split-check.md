# split-check: split_check.py proves a file split moved code and changed none

Risk: low. The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status split-check`.

## Outcome

Owner, 2026-11-21: "Any 6K file is just unmaintainable at that size. Source code files should be
much smaller and well organized." A split moves code and changes none: bodies, names, order inside
a function and behavior stay as they were.
`cpp-libraries/tools/split_check.py [--base REF] [--max-lines N] PATH...` proves it mechanically for
every Kronuz C++ repository: between `git merge-base REF HEAD` and HEAD, under PATHs, every
non-blank line removed from a file reappears among the added lines, and every added line that was
not removed is one of the allowed shapes: `#include`, `#pragma once`, a namespace opening or
closing, a blank line, a comment line, or a forward declaration. Anything else fails, naming the
file and line. It also fails when a file under PATHs exceeds N lines (default 1,000), and it warns
(listing them) for lines whose only change is a dropped `static` or an anonymous namespace (a
linkage change) and for namespace-scope objects with dynamic initializers that changed translation
unit. Whitespace-only differences are ignored. Standard library only, Python 3.9+.

## Acceptance criteria

- A1 A pure split of a fixture file into two passes; a changed line, a dropped line, an added
  statement and a file over the limit each fail naming the file and line; a dropped `static` and a
  moved dynamic initializer are warned about: tests with values in `tools/test_split_check.py`, the
  `test` check
- A2 `tools/split_check.py` passes preflight's ruff and pyrefly configuration (tools/ruff.toml,
  tools/pyrefly.toml): a job
- A3 [artifact] `tools/README.md` names the tool and its command
- A4 [ci] CI is green on every platform

## Contracts

Provides cpp-libraries:split-check, consumed by every split quest (schema-split, split-*).

## Decisions

- Lead, 2026-11-21: one shared tool in cpp-libraries, not one per repository; schema-split's
  planned `harness/split_check.py` is replaced by it.

## May land early

Nothing: one landing.
