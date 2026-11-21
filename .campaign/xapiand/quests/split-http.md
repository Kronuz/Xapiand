# split-http: http's large header and tests split where it helps maintenance

Risk: low. The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status split-http`.

## Outcome

Owner, 2026-11-21, later: "The split must make sense and improve maintainability. Let's not
split for the sake of splitting. For example, a 10K file might be ok if it makes complete sense to
keep it that way." The sizes below flag a file for a look; they are not the reason to split it.

Owner, 2026-11-21: "Any 6K file is just unmaintainable at that size. Source code files should be
much smaller and well organized." A split moves code and changes none: bodies, names, order inside
a function and behavior stay as they were.

Files: `http_asio.h` (1,281 lines), `test/coroutine_test.cc` (1,364), `test/readahead_test.cc`
(1,241). `http_asio.h` stays the include its users (Xapiand, Detent) name. Detent's lead is told
before it starts.

## How to split

- Split only along boundaries a maintainer recognizes: separate classes, algorithm families,
  subsystems, test topics. Each piece is a unit someone opens alone to work on one thing, and it
  opens with a one-line comment saying what. Size is never the reason for a cut.
- A file that is one coherent unit stays whole at any size: one function or state machine, one
  class whose members only make sense together, a table, a test of one behavior. The layout note
  names it and why. A quest may split none of its files.
- No piece is a fragment included inside a function, switch, class or namespace body, and no piece
  is too small to stand alone: under about 100 lines, it joins its natural neighbor.
- A header keeps its path as the one include for its callers (it includes its pieces), so no caller
  changes. A test file keeps its build target and its ctest names: its pieces are added to the same
  target, so `ctest -N` lists the same tests before and after.
- A helper that several pieces now share moves to an internal header in a `detail` namespace; that
  linkage change is listed in the journal. A namespace-scope object with a dynamic initializer stays
  in the translation unit that defines what it depends on, or becomes a function-local static, so
  static initialization order cannot change.
- One commit per split file, so a conflict with another branch is found per file.

## Acceptance criteria

- A1 The suite passes at the split code: the suite check
- A2 Every moved line moved unchanged: the `split`
  check (cpp-libraries' `tools/split_check.py`), with its warnings answered in the journal
- A3 `ctest -N` lists the same tests before and after: a job at each commit, diffed
- A4 [artifact] the layout note (`docs/source-layout.md`) lists each
  new file and what it holds
- A5 [ci] CI is green on every platform
- A6 Every header this quest creates compiles alone, checked the way cpp-libraries'
  `tools/selfcontained.sh` checks one (a translation unit that only includes it twice,
  `-fsyntax-only`), over the new headers only: a job at the head
- A7 [manual] The lead read the split: each piece is a unit a maintainer opens alone, and the
  layout note names each file kept whole over 1,000 lines and why

## Contracts

None: nothing callers see changes.

## May land early

Each split file on its own, once A1, A2 and CI hold for it.
