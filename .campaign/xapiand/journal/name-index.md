# name-index: journal

State: working
Quest: .campaign/xapiand/quests/name-index.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 1 of docs/design/name-index.md built (4fc89cd..a3058e2): `NameIndex` (src/database/name_index.{h,cc}),
resolve's hook (src/database/pool.cc), the search's use (src/server/search_views.cc), tests in
tests/test_index_pattern.cc (A3 property, A4 unit, bench for A2) and tests/test_node.cc (A4 node, A5 kill),
mutants in tests/name_index_mutants.sh. Not compiled locally (host loaded); jobs below build it.

Jobs: 261117-080546-47c0ee targeted build + new tests (at 4fc89cd); build-test 261117-080830-d0bdd7 and e2e
261117-080830-465be1 (at acde48f); A2 benches 261117-080833-2abb8f (10^4, 10^5) and 261117-080839-4055d7 (10^6),
strict, after the targeted job.

## Next

1. Targeted job green, then the mutants as --fails-without jobs: no-parents, keep-slash, drop-delta, lose-merge
   (A3, test_index_pattern), no-seek, no-range (A2/A4, test_a_prefix_pattern_reads_its_range,
   test_the_names_answer_over_the_cap), no-index, no-hook (A4, test_node over_cap), top-build (A5, test_node names).
2. Bench numbers into docs/design/name-index.md "Costs"; re-verify at the final head.

## Blocked on

Nothing blocking. For the lead (outside the file set): docs/src/content/docs/options.md and changelog say a
pattern is refused for "directory entries"; with the index it is also refused for "index names" read. Starting
the build at the node's start needs src/manager.cc (design step 3). The item split in NameIndex::expand repeats
index_pattern::expand's (about 25 lines), since src/index_pattern.{h,cc} is outside the file set; folding it
into one parse step needs that file.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | `docs/design/name-index.md` |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- R1 In memory, rebuilt by one walk at every start; no persisted table (design D1). The directory stays the one
  truth, so A5 has nothing to reconcile.
- R2 The build starts at the node's first pattern (lazy), not at boot: src/manager.cc is outside the file set, and
  a node that serves no pattern pays nothing. Until it ends, patterns walk.
- R3 The lookup keeps `--max-search-indexes` as its bound, counting names read and seeks as visits (D7); `_all`
  over the cap stays 400. The walk gate does not apply to lookups.
- R4 Resolve adds on open as well as create, so an index copied in while running is listed once opened (D3).
