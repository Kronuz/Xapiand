# split-docset: docset's large files split where it helps maintenance

Risk: low. The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status split-docset`.

## Outcome

Owner, 2026-11-21, later: "The split must make sense and improve maintainability. Let's not
split for the sake of splitting. For example, a 10K file might be ok if it makes complete sense to
keep it that way." The sizes below flag a file for a look; they are not the reason to split it.

Owner, 2026-11-21: "Big files is generally unacceptable." A split moves code and changes none:
bodies, names, order inside a function and behavior stay as they were. Files (sizes at the quest's
filing):

- `docset.hh` (1,455 lines)

Not split: generated code, vendored third-party code, and frozen reference copies kept for
differential tests (`*_old.*`, `test/old/`, `frozen_*`): they are never edited by hand.

## How to split

- Split only along boundaries a maintainer recognizes: separate classes, algorithm families,
  subsystems, test topics. Each piece is a unit someone opens alone to work on one thing, and it
  opens with a one-line comment saying what. Size is never the reason for a cut.
- A file that is one coherent unit stays whole at any size: one function or state machine, one
  class whose members only make sense together, a table, a test of one behavior. The layout note
  names it and why. A quest may split none of its files.
- No piece is a fragment included inside a function, switch, class or namespace body, and no piece
  is too small to stand alone: under about 100 lines, it joins its natural neighbor.
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
