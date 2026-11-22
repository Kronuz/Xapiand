# split-tooling: Split tooling: function sizes on Mach-O, macro-body locals, test trees, the ctest -N recipe

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status split-tooling`.

## Outcome

Four gaps the 2026-11-21 splits hit in cpp-libraries' split tooling are closed. Each one cost
contributors jobs or silently proved nothing (hunt rows 392, 393, 394, 400):

- A size comparison works on macOS. `nm -S` prints size 0 for every Mach-O symbol, so
  SPLITTING.md's `nm -S --size-sort` recipe compared names only (split-xapiand-src's first A3
  jobs, 261121-121104-e6ee83 and 261121-121123-22c397).
- `split_check.py` no longer reports a local inside a macro-opened function body as a moved
  namespace-scope dynamic initializer (split-xapiand-tests: tests' lua `method.cc:148`).
- `selfcontained.sh` can check headers under `tests/`.
- SPLITTING.md gives one `ctest -N` diff recipe that ignores ctest's "could not find
  executable" lines. Four splits each lost a job to those lines.

## Acceptance criteria

- A1 A function-size tool (`tools/fnsizes.sh` or a mode of an existing tool) prints nonzero
  per-function sizes for a Release object or binary on macOS (Mach-O, size = distance to the next
  text symbol in the same section) and on Linux (ELF, `nm -S`). Two runs over one build print
  the same table. SPLITTING.md's "Moves only" section names it in place of the `nm -S` recipe:
  the `test` check (a test over a small fixture binary built in the test)
- A2 A local with a dynamic initializer inside a function whose body a macro opens (the
  `method.cc:148` shape, reduced to a fixture) produces no moved-initializer warning, and a
  moved namespace-scope dynamic initializer still warns: the `test` check
- A3 `selfcontained.sh` takes a flag (or a glob) that includes `tests/` and `test/` headers. It
  stays off by default, so existing callers see no change: the `test` check
- A4 [artifact] `docs/SPLITTING.md` "What lands a split" item 3 gives the `ctest -N` recipe that
  keeps only `Test #` and `Total Tests` lines, and the function-size recipe of A1
- A5 ruff check and ruff format pass on every tool touched, the existing tool tests pass, and CI
  is green: the `lint` and `test` checks; [ci]

## Contracts

`split_check.py`'s command line and exit codes stay as they are: every split quest runs it from
cpp-libraries' main checkout, and 22 of their gate checks are queued against that path.
`selfcontained.sh`'s default behavior stays as it is.

## Decisions

- Lead, 2026-11-21: one quest for the four rows, since they share one repository, one doc and one
  reviewer's read. Low risk: tools and docs, with no product code.

## May land early

Nothing: one landing.

## Tasks

None: one contributor (maintenance, economical, medium).
