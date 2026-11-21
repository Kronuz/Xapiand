# split-xapiand-core: Xapiand's request and database sources split where it helps maintenance

Risk: low. The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status split-xapiand-core`.

## Outcome

Owner, 2026-11-21, later: "The split must make sense and improve maintainability. Let's not
split for the sake of splitting. For example, a 10K file might be ok if it makes complete sense to
keep it that way." The sizes below flag a file for a look; they are not the reason to split it.

Owner, 2026-11-21: "Any 6K file is just unmaintainable at that size. Source code files should be
much smaller and well organized." A split moves code and changes none: bodies, names, order inside
a function and behavior stay as they were.

Files: `src/database/handler.cc` (3,470 lines), `src/server/search_views.cc` (3,360),
`src/database/shard.cc` (1,943), `src/engine/strata/shard.cc` (1,657), `src/query_dsl.cc` (1,648),
`src/manager.cc` (1,410), `src/main.cc` (1,176), `src/database/pool.cc` (1,047), and the tests
`test_node.cc` (3,885), `test_pool.cc` (2,508), `test_engine_strata.cc` (1,310),
`test_loop_yield.cc` (1,273), `test_index_pattern.cc` (1,181). It starts after journal-replaces-wal,
which rewrites most of them (`src/database/wal.cc` goes with the WAL, so it is not split), and when
no other active quest holds them. Sizes are at Xapiand:b29ab1f92; recount at the start.

## How to split

- Follow cpp-libraries' `docs/SPLITTING.md` (cpp-libraries:7cab8ec): when to split, where to cut,
  what stays whole, moves only, and what lands a split.

## Acceptance criteria

- A1 The suite passes at the split code: the suite check
- A2 Every moved line moved unchanged: the `split`
  check (cpp-libraries' `tools/split_check.py`), with its warnings answered in the journal
- A3 Release builds use LTO (CMakeLists.txt `-flto` for RELEASE), so moved functions keep their
  inlining: the moved functions' sizes in a Release build (`nm -S --size-sort`) match before and
  after, or each difference is listed and explained: a job at each commit
- A4 [artifact] the layout note (`docs/dev/source-layout.md`) lists each
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
