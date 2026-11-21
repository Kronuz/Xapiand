# split-check-sources: split_check compares only C and C++ sources

Risk: low. The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status split-check-sources`.

## Outcome

Every split adds its layout note (`docs/source-layout.md`) and often a CMake line, and
`tools/split_check.py` judged those as code: split-storage's check failed on its layout note's
lines (job 261121-122326-a913a0). The moves-only checks now look only at C and C++ sources (the
suffixes the size note already uses); a line moved into any other file, an `.inc` fragment
included, still counts as lost.

## Acceptance criteria

- A1 A split that adds a Markdown layout note and a CMake source line passes, and a line moved
  from a `.cc` into an `.inc` fails as lost: the `test` check
- A4 An include guard on a new header (`#ifndef`/`#define` of a guard-like name and its `#endif`)
  passes, and any other new `#define` fails (split-btree's 18 guard lines, job 261121-123001-f68c62):
  the `test` check
- A5 A static helper becoming `inline`, `inline` added to a moved definition, and a namespace-scope
  `using` of a detail namespace pass with a warning; `using namespace std;` and a `using` inside a
  function still fail; a dropped include or comment is a warning (the 2026-11-21 split checks:
  http, xapiand-src, xapiand-tests): the `test` check
- A2 ruff check, ruff format and the existing tests pass: the `lint` and `test` checks
- A3 [ci] CI is green

## Contracts

None.

## May land early

Nothing: one landing.
