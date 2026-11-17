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

0. A5 (P7), chosen by gap (track-p umbrella: never set ready for review). Latest comparison at
   main is job 261117-075402-484d70 at prospect:0aad777 (main unchanged since), over `serve`
   against Tantivy 0.26. Rows still slower (prospect/Tantivy): negated 2 `TOP_100` 1.183,
   `TOP_1000` 1.155, `TOP_100_COUNT` 1.637, `COUNT` 1.147; negated 3 1.177, 1.176, 1.199, 1.279;
   phrases 1.16-1.28 (`TOP_10` 1.00-1.13); intersection 2 1.080/1.145/1.175; intersection 3
   1.078/1.132 (`TOP_1000`, `TOP_100_COUNT`); intersection_union `TOP_1000` up to 1.175.
   Why P7: the largest ratios are the negations, and they point at P7 (membership probes on the
   excluded terms, left "not applied" in `p7-membership-probes.md`). Phrases point at P8/P5:
   six read-path trims already lost (`phrases-explore.md`, `phrase-speed.md`) and what remains is
   a format change (pair postings or P8's prefilter section), too large for this budget. P3,
   P11 and P21 have no losing row (terms and unions win). Cause found on main: a negation's
   top-k has no bulk path; it walks every included posting, steps the excluded terms to it and
   scores it (`+jordan -country` `TOP_100` 422 us against `jordan` alone 83 us).
   Variants (`PROSPECT_EXPERIMENT_P7`): bit 1, the term's bulk top-k with excluded clauses
   probed only for hits that could enter; bit 2, nested exclusions counted apart by windows so
   a counted top-k takes the bulk path; bit 4, term-minus-term counted by windows too.
1. Disk: host at 96%; P9/P13 builds and `.scratch/idx-p13` removed.

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

A10 (P13): `docs/perf/p13-positions.md` (prospect:e8ea840), removed. EF per block: segment
-1.04%, phrase rows 0.99-1.10 of main with four losing (job 261117-032504-3bf776, A/A from main's
copy), serve agrees (261117-032507-1bd2ec), indexing +2.6% (261117-032510-cabfbf), decode 2.0-2.5x
PFOR's (261117-032048-bef4fe), ctest 52/52 with the patch (261117-032521-37a35e). Rice from length:
-6.6% of the segment at 16x PFOR's decode, removed offline. Main vs Tantivy on phrases 1.10-1.40.

A7 (P9): `docs/perf/p9-planner.md` (prospect:7754048), removed. Baseline 261117-075402-484d70 ranks
P9's rows as the losers vs Tantivy (P3's all win). Windowed top-k 1.12-1.34 (loser); match first
0.93-0.95 at TOP_100/1000, 1.04-1.11 at TOP_10; adaptive (bit 4) 0.88-1.03 run level, per-query
best 0.95-0.99, inside the A/A (383ae4; serve ad33ee agrees); ctest passed (2768e6). Parity clean
for every variant. Patch `docs/perf/p9-planner.patch` is the record.

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
- A10: verdict by the plan's rule, the A/A band read as the A/A run's interval: a row loses when its
  lower bound is over 1.0 and its ratio over the A/A's upper bound. The worktree's applied patch was
  reverted (it equals `docs/perf/p13-positions.patch`, which stays as the record).
- A7: the deciding numbers come from 383ae4 and ad33ee although they exited 1: the failing step was
  the scripts' own file-count check after the tables were complete (padded `wc -l`), not the runs.
- A1 follow-up: a win also needs its ratio under the A/A's lower bound (as A10's loss rule);
  a short-list case is within its A/A when its median is inside the A/A's per-round range.
- A1 follow-up: the selector branch only chooses between two correct paths, so no mutant of it
  can be caught by values; mutants cover the probes, the row compare and its bounds; the test's
  skewed blocks take both branches with the estimate wrong each way.
- A1 follow-up: removed, per the lead's rule: the gain must hold beyond the A/A at the landing
  commit; it held on three rows there, but none of the screen's three, so no row replicated.
- A7: P9 stops after three experiments (two losers and one non-winner); the remaining gap to Tantivy
  is `advance` and scoring every lead document, not evaluation order.
