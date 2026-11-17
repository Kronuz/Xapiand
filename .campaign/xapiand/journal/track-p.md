# track-p: journal

State: waiting on jobs 261117-032048-bef4fe 261117-032504-3bf776 261117-032507-1bd2ec 261117-032510-cabfbf 261117-032521-37a35e 261117-023209-cd38c4
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

0. A10 (P13): prospect `docs/perf/p13-positions.md` (plan, rule, offline bytes), tool
   `bench/quick/p13_positions.cc`, prototype `docs/perf/p13-positions.patch` (EF per block when
   smaller, header 0xC0|l, `PROSPECT_EXPERIMENT_P13=1`), variant 1M index `.scratch/idx-p13`
   (positions 342.13 MB vs 348.09, file 570.32 vs 576.29 MB, -1.04%), parity clean (dump both,
   verify 0 mismatches, 962 queries). Jobs: bef4fe decode microbench, 3bf776 A/B (deciding),
   1bd2ec engines over serve, cabfbf indexing speed, 37a35e ctest with the patch. When they land:
   write the A/B, engines, four-axis table and verdict into the entry; Rice-from-length stays an
   offline result (-11.0% positions, decode about 12x PFOR's in the screen).
   Lead's ruling: keep EF only if 3bf776 shows a query-speed win beyond the A/A spread; else record
   it removed, Rice removed too; no more positions experiments. Then: the next Track P item with
   the largest measured gap to Tantivy (P3 or P9), from a fresh baseline.
1. Jobs running/queued: 261117-023209-cd38c4 (before/after vs Lucene/Tantivy over serve, exclusive),
   261117-024522-d2facd (full ctest with the patch at bits 1+2), 261117-024524-7249b7 (build-test).
   Add the after-vs-Tantivy table to `docs/perf/p2-intersection.md` when cd38c4 lands.
2. A1 landing answered (lead): quest p2-intersection (prospect:5227e00) lands bits 1+2.

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

`docs/perf/p2-intersection.md` (prospect): P2 kept in part. Bits 1+2 (branch-free compare of all
128 slots of a decoded block; per-word rank in bitset blocks) run the intersection classes at
0.78-0.92 of main's time at 1M (job 261117-021758-5aded6, exclusive, 7 rounds, A/A 0.97-1.04),
guard classes 0.82-1.0, none lost; parity clean. Galloping across blocks lost; block-against-block
filtering for counted conjunctions neutral; both removed. Baseline (job 261117-021317-afb4da):
main beats Lucene on every intersection row; Tantivy leads main at TOP_100/TOP_1000/TOP_100_COUNT
(1.10-1.30). Prototype: `docs/perf/p2-intersection.patch`; driver `bench/quick/classes_ab.py`.
Not measured: arm64, Linux, FreeBSD, the gate VM's 5M, PISA (not on this host).

## Rulings I made

- A10: proximity is not measured: prospect's harness queries have no proximity class (no NEAR in
  the gate set); phrase classes stand for P13's query axis.
- A10: the length-based Rice code is judged offline (bytes, decode microbench, profile share), not
  prototyped: it is bit-serial per document and cannot step over blocks, and its screened decode
  is about 12x PFOR's against a 7-10% decode share of phrase rows.
- A10: landing either codec bumps strata's `pre_freeze` (0x8000'000D to 0x8000'000E) with fixtures,
  digest identity, a decoder fuzz target and a revert check (track-p Contracts, D30).

- The prototype stays a patch in `docs/perf/` (source files are outside the file set); jobs apply
  it in their checkout.
- Instrument: in-process `bench` for A/Bs, `serve` round trips for engine comparisons (pipe cost
  dilutes both sides alike); 1M rather than 5M (AGENTS.md: iterate small, one large run confirms).
- Bit 2 kept with bit 1 although it wins one row alone: it leans below 1.0 on every row, costs
  64 bytes per iterator and one store per word at load.
- Stopped at four experiments: two non-winners in a row after the kept ones; the profile's
  remainder is the conjunction's scoring loop and the collector, not P2's mechanisms.
