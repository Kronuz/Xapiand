# split-http: http's large header and tests split under 1,000 lines

Risk: low. The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status split-http`.

## Outcome

Owner, 2026-11-21: "Any 6K file is just unmaintainable at that size. Source code files should be
much smaller and well organized." A split moves code and changes none: bodies, names, order inside
a function and behavior stay as they were.

Files: `http_asio.h` (1,281 lines), `test/coroutine_test.cc` (1,364), `test/readahead_test.cc`
(1,241). `http_asio.h` stays the include its users (Xapiand, Detent) name. Detent's lead is told
before it starts.

## How to split

- Split by responsibility (what the code does), not by line count: each new file holds one thing and
  opens with a one-line comment saying what. Aim for 200 to 600 lines; never over 1,000.
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
- A2 Every moved line moved unchanged and no file in the quest's set exceeds 1,000 lines: the `split`
  check (cpp-libraries' `tools/split_check.py`), with its warnings answered in the journal
- A3 `ctest -N` lists the same tests before and after: a job at each commit, diffed
- A4 [artifact] the layout note (`docs/source-layout.md`) lists each
  new file and what it holds
- A5 [ci] CI is green on every platform
- A6 Every header under the library's public paths compiles alone: cpp-libraries'
  `tools/selfcontained.sh -q .` as a job at the head

## Contracts

None: nothing callers see changes.

## May land early

Each split file on its own, once A1, A2 and CI hold for it.
