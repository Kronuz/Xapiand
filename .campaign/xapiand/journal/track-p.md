# track-p: journal

State: waiting on jobs 261117-023209-cd38c4 261117-024522-d2facd 261117-024524-7249b7
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

1. Jobs running/queued: 261117-023209-cd38c4 (before/after vs Lucene/Tantivy over serve, exclusive),
   261117-024522-d2facd (full ctest with the patch at bits 1+2), 261117-024524-7249b7 (build-test).
   Add the after-vs-Tantivy table to `docs/perf/p2-intersection.md` when cd38c4 lands.
2. Landing (lead): bits 1+2 into `include/prospect/reader.hh` without the switch; see Blocked on.

## Blocked on

Lead: landing the kept change needs `include/prospect/reader.hh` (outside track-p's file set
`docs/perf/**, bench/quick/**`). Options: (a) widen the file set or open a P2 landing quest, which
applies bits 1 and 2 of `docs/perf/p2-intersection.patch` without the `#if`, adds a unit test for
in-block advance on short tails and bitset ranks, and runs CI (arm64, Linux, FreeBSD) once;
(b) leave the entry as the verdict only. Recommend (a).

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

`docs/perf/p2-intersection.md` (prospect): P2 kept in part. Bits 1+2 (branch-free compare of all
128 slots of a decoded block; per-word rank in bitset blocks) run the intersection classes at
0.78-0.92 of main's time at 1M (job 261117-021758-5aded6, exclusive, 7 rounds, A/A 0.97-1.04),
guard classes 0.82-1.0, none lost; parity clean. Galloping across blocks lost; block-against-block
filtering for counted conjunctions neutral; both removed. Baseline (job 261117-021317-afb4da):
main beats Lucene on every intersection row; Tantivy leads main at TOP_100/TOP_1000/TOP_100_COUNT
(1.10-1.30). Prototype: `docs/perf/p2-intersection.patch`; driver `bench/quick/classes_ab.py`.
Not measured: arm64, Linux, FreeBSD, the gate VM's 5M, PISA (not on this host).

## Rulings I made

- The prototype stays a patch in `docs/perf/` (source files are outside the file set); jobs apply
  it in their checkout.
- Instrument: in-process `bench` for A/Bs, `serve` round trips for engine comparisons (pipe cost
  dilutes both sides alike); 1M rather than 5M (AGENTS.md: iterate small, one large run confirms).
- Bit 2 kept with bit 1 although it wins one row alone: it leans below 1.0 on every row, costs
  64 bytes per iterator and one store per word at load.
- Stopped at four experiments: two non-winners in a row after the kept ones; the profile's
  remainder is the conjunction's scoring loop and the collector, not P2's mechanisms.
