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

1. Jobs 261117-021317-afb4da (baseline vs Lucene/Tantivy, serve) and 261117-021758-5aded6 (deciding
   A/B: main, A/A, bit 1, bits 1+2; intersection and guard classes) queued, exclusive.
2. With their numbers: write the Result and four-axis table in `docs/perf/p2-intersection.md`.
3. Landing bit 1 (and 2 if it pays) needs `include/prospect/reader.hh`, outside the file set: Blocked on.

Screens so far (`quiet strict`, 5 rounds, per-query best vs main; parity clean for 3 and 11):
bits 1+2 0.80-0.89 on every intersection class and command, guard classes 0.82-1.0, terms ~1.0;
bit 1 alone 0.84-0.96; bit 2 alone 0.93-1.06 (noise); bit 4 (gallop) 0.95-1.11, a loser; bit 8
(block filter for counted conjunctions) over 1+2: 0.95-1.06, neutral.

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
