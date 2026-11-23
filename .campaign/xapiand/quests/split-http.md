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

- Follow cpp-libraries' `docs/SPLITTING.md` (cpp-libraries:7cab8ec): when to split, where to cut,
  what stays whole, moves only, and what lands a split.

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
