# split-storage: storage's large files split under 1,000 lines

Risk: low. The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status split-storage`.

## Outcome

Owner, 2026-11-21: "Big files is generally unacceptable." A split moves code and changes none:
bodies, names, order inside a function and behavior stay as they were. Files (sizes at the quest's
filing):

- `storage.h` (1,753 lines)

Not split: generated code, vendored third-party code, and frozen reference copies kept for
differential tests (`*_old.*`, `test/old/`, `frozen_*`): they are never edited by hand.

## How to split

- By responsibility, not line count: each new file holds one thing and opens with a one-line
  comment saying what. Aim for 200 to 600 lines; never over 1,000.
- A header keeps its path as the include its users name (Xapiand, Detent and the other libraries):
  it includes its pieces, so no user changes. Installed headers stay installed (CMake install rules
  and the package's include directory list the pieces).
- Test pieces join their existing test target: `ctest -N` lists the same tests before and after.
- A helper several pieces share moves to an internal header in a `detail` namespace; a
  namespace-scope object with a dynamic initializer stays with what it depends on. Each such
  non-move goes under the journal's "Rulings I made".
- Every new header is self-contained: it includes what it uses (its siblings and the standard
  headers) and compiles alone, so a piece is a real file, not a fragment of its umbrella.
- One commit per split file.

## Acceptance criteria

- A1 The library's tests pass at the split code: the `build-test` check
- A2 Every moved line moved unchanged and no file in the quest's set exceeds 1,000 lines: the `split`
  check (cpp-libraries' `tools/split_check.py`), its warnings answered in the journal
- A3 `ctest -N` lists the same tests before and after: a job at each commit, diffed
- A4 [artifact] `docs/source-layout.md` lists each new file and what it holds
- A5 [ci] CI is green on every platform
- A6 Every header this quest creates compiles alone, checked the way cpp-libraries'
  `tools/selfcontained.sh` checks one (a translation unit that only includes it twice,
  `-fsyntax-only`), over the new headers only: a job at the head

## Contracts

None: the include paths and everything users see stay the same.

## May land early

Nothing: one landing.
