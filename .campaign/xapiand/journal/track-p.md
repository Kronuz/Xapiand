# track-p: journal

State: working
Quest: .campaign/xapiand/quests/track-p.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A1 (P2), performance assignment, code in prospect worktree `prospect-wt/xapiand/track-p` (branch
quest/xapiand/track-p). Setup on the Intel i9-9980HK host: 1M gate corpus
(`strata-wt/index-size/.scratch/corpus-1000000.json`), 962 gate queries split per class by
`bench/quick/classes_ab.py split` into `.scratch/q/`; Lucene 10.4 and Tantivy 0.26 single-thread
engines copied into `.scratch/engines/` and indexed at `.scratch/idx-{lucene,tantivy}`; prospect main
built at `build-rel` (-march=native, the harness's flags). PISA is not installed on this host.

## Next

1. Baseline: `classes_ab.py serve` prospect main vs Lucene vs Tantivy on the intersection classes,
   exclusive job, 5 rounds.
2. Profile intersection classes (`sample`); experiments behind a switch: bitset rank directory,
   galloping across blocks, SIMD count_below / block intersection.
3. Interleaved A/B with A/A (`classes_ab.py bench`), exclusive, >= 5 rounds; verify + dump parity.
4. `docs/perf/p2-intersection-simd.md` with the four-axis table.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>
