# split-xapiand-src: Xapiand's free large sources split under 1,000 lines

Risk: low. The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status split-xapiand-src`.

## Outcome

Owner, 2026-11-21: "Any 6K file is just unmaintainable at that size. Source code files should be
much smaller and well organized." A split moves code and changes none: bodies, names, order inside
a function and behavior stay as they were.

Files: `src/serialize.cc` (1,696 lines), `src/aggregations/metrics.h` (1,265),
`src/aggregations/bucket.h` (1,262), `src/lua/processor.cc` (1,125). No active quest holds them.

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

Pieces stay in their file's directory with its name as prefix (`src/serialize_*.cc`,
`src/aggregations/metrics_*.h`): CMake globs sources per directory, so no build file changes.

## Acceptance criteria

- A1 The suite passes at the split code: the suite check
- A2 Every moved line moved unchanged and no file in the quest's set exceeds 1,000 lines: the `split`
  check (cpp-libraries' `tools/split_check.py`), with its warnings answered in the journal
- A3 Release builds use LTO (CMakeLists.txt `-flto` for RELEASE), so moved functions keep their
  inlining: the moved functions' sizes in a Release build (`nm -S --size-sort`) match before and
  after, or each difference is listed and explained: a job at each commit
- A4 [artifact] the layout note (`docs/dev/source-layout.md`) lists each
  new file and what it holds
- A5 [ci] CI is green on every platform
- A6 Every header under the library's public paths compiles alone: cpp-libraries'
  `tools/selfcontained.sh -q .` as a job at the head

## Contracts

None: nothing callers see changes.

## May land early

Each split file on its own, once A1, A2 and CI hold for it.
