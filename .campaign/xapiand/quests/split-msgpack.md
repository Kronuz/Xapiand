# split-msgpack: msgpack's large files split where it helps maintenance

Risk: low. The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status split-msgpack`.

## Outcome

Owner, 2026-11-21, later: "The split must make sense and improve maintainability. Let's not
split for the sake of splitting. For example, a 10K file might be ok if it makes complete sense to
keep it that way." The sizes below flag a file for a look; they are not the reason to split it.

Owner, 2026-11-21: "Big files is generally unacceptable." A split moves code and changes none:
bodies, names, order inside a function and behavior stay as they were. Files (sizes at the quest's
filing):

- `msgpack.h` (3,575 lines)
- `test/diff_test.cc` (1,026 lines)

Not split: generated code, vendored third-party code, and frozen reference copies kept for
differential tests (`*_old.*`, `test/old/`, `frozen_*`): they are never edited by hand.

## How to split

- Follow cpp-libraries' `docs/SPLITTING.md` (cpp-libraries:7cab8ec): when to split, where to cut,
  what stays whole, moves only, and what lands a split.

## Acceptance criteria

- A1 The library's tests pass at the split code: the `build-test` check
- A2 Every moved line moved unchanged: the `split`
  check (cpp-libraries' `tools/split_check.py`), its warnings answered in the journal
- A3 `ctest -N` lists the same tests before and after: a job at each commit, diffed
- A4 [artifact] `docs/source-layout.md` lists each new file and what it holds
- A5 [ci] CI is green on every platform
- A6 Every header this quest creates compiles alone, checked the way cpp-libraries'
  `tools/selfcontained.sh` checks one (a translation unit that only includes it twice,
  `-fsyntax-only`), over the new headers only: a job at the head
- A7 [manual] The lead read the split: each piece is a unit a maintainer opens alone, and the
  layout note names each file kept whole over 1,000 lines and why

## Contracts

None: the include paths and everything users see stay the same.

## May land early

Nothing: one landing.
