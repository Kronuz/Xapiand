# prospect-supplied-stats: journal

State: ready for review
Quest: .campaign/xapiand/quests/prospect-supplied-stats.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Implemented `supplied_statistics`, `term_statistics`, `gather_statistics`, `term_table(segs, &supplied)` in
`include/prospect/searcher.hh`; `test/stats_test.cc` (A1), `test/stats_mutants.py` (A2),
`test/stats_ab_build.sh` + `test/stats_ab.sh` (A3), `docs/mechanisms/supplied-statistics.md` (A4).
Compiling and iterating stats_test locally via `quiet light` in build/.

## Next

1. Read job results; fold failures. Jobs at prospect:2202030 (stats_test passed locally at the pre-commit tree, thresholds then lowered):
   A1 261115-232146-62bd5a; A2 mutant 261115-232149-29b412 (fails-without), all mutants 261115-232153-05152e;
   build-test 261115-232155-edc5bb; A3 build 261115-232202-ddb7cf then A/A 261115-232208-acfc3c, A/B 261115-232211-560561.
2. Fill Evidence and Result; state ready for review.

## Blocked on

<What, and who decides. "Nothing" when nothing.>

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | 261115-232146-62bd5a |
| A2 | 261116-004953-d3a74c (`--fails-without`; first run 261115-232149-29b412 untagged) |
| A3 | 261115-232155-edc5bb, 261115-232211-560561 (A/A 261115-232208-acfc3c) |
| A4 | `docs/mechanisms/supplied-statistics.md` |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

1. CMakeLists.txt gets the `stats_test` target (outside the quest's file set, needed to build the test).
2. `multi_snapshot` (shards.hh, outside the file set) is untouched: a host with supplied statistics weighs
   through `term_table(snap.segments(), &supplied)`; `multi_snapshot::stats()/df()` stay local.
3. A term the shard holds and the supplied set lacks throws (errors never pass silently); absent from both is df 0.
4. cf is carried and returned (`term_table::cf`); no shipped weight reads it.
