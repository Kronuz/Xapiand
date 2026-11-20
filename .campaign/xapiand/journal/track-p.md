# track-p: journal

State: waiting on jobs 261119-165425-f30bdd 261119-165432-b1efdc 261119-165438-2cf08a 261119-165439-7fcba3 261119-165445-56d539 261119-165452-b53291 261119-165454-58af8f 261119-165506-ec3eaa 261119-165513-aa1969 261119-165517-69db0e 261119-165524-86e55d
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

0. Session 2026-11-19 (resumed, fresh context): main unchanged (prospect:0aad777; origin/main has
   only a CI commit), no merge. P7 landing jobs all passed; 9088e8 recorded in
   `docs/perf/p7-exclusion.md` (prospect:162b5d0, docs only): five negated rows 0.39-0.79, short
   `TOP_10` 0.80, intersection 2 `TOP_100` 0.989 (cleared), no class row lost; one short case
   still loses (`+xylophone -music -film` `TOP_1000` 1.31, 28.2 -> 34.7 us: k above its 28
   matches, nothing to prune). Untested fix idea: keep the generic loop when hits collected plus
   the term's df cannot reach k.
   Stale evidence resubmitted at prospect:2ce63ac with `--for` tags (same scripts and levels):
   A5 mutant 3 f30bdd (--fails-without); A7 ctest b1efdc, A/B 2cf08a, engines 7fcba3 (exclusive);
   A10 ctest 56d539, micro b53291, index 58af8f, idx-p13 rebuild ec3eaa (light, untagged) then
   A/B aa1969 and engines 69db0e (exclusive, after ec3eaa); A14 gap rows at head 86e55d
   (exclusive; builds `.scratch/bin/head-2ce63ac`, serve vs Lucene/Tantivy, every class).
   `campaign verify track-p`: build-test reused (ecb275, docs-only since).
   Next: read each job; for A10/A7 confirm the verdicts still hold (numbers in the docs' tables
   need no change unless they move); write A14's gap table (negated rows vs Tantivy at head)
   into `p7-exclusion.md`'s Against-Tantivy paragraph; paste the brief's Evidence table.
1. Disk: host at 96% (43 GB free); idx-p13 (1.1 GB) is rebuilt for the reruns: remove it after.

## Blocked on

Lead: A2-A4, A6, A8, A9, A11-A13 (P3, P4, P5, P8, P10, P11, P14, P20, P21) are unstarted research
items, not stale evidence: no rerun can give them a row, so "ready for review" cannot be set for
the whole quest. Options: (a) land track-p for A1, A5, A7, A10, A14 now and split each remaining
item into its own quest (the quest's Outcome: "the lead splits it into one quest per item");
(b) keep track-p open as the umbrella and its file hold on automata and phase5. Recommend (a):
it frees the held files and matches the quest's own rule.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A5 | P7 kept (bit 1), `docs/perf/p7-exclusion.md` (prospect:162b5d0): deciding A/B 261117-140026-dba8cc; at landing prospect:bd60f39: suite 261117-155032-ecb275, parity 261117-155033-63b78b, A/B 261117-155036-9088e8 (passed); mutants 261117-155040-04a4e1, 261117-155044-c780b6, 261117-155047-253843 (caught), mutant 3 261119-165425-f30bdd (pending); prototype mutants 261117-141718-c0035a, 261117-141721-81b492, 261117-141725-aa9027 |
| A7 | P9 removed, `docs/perf/p9-planner.md`: reruns at head pending 261119-165432-b1efdc, 261119-165438-2cf08a, 261119-165439-7fcba3 |
| A10 | P13 removed, `docs/perf/p13-positions.md`: reruns at head pending 261119-165445-56d539, 261119-165452-b53291, 261119-165454-58af8f, 261119-165513-aa1969, 261119-165517-69db0e |
| A14 | gap rows at head pending 261119-165524-86e55d |

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

- A5: search.hh is outside the file set; productionized there on the lead's message (P7 verdict).
- A5: mutants that only change work, not values (the threshold drop's `<=`, restarts always
  on, the started flag) are not listed: no value test can catch them.
- A5: the short-list probe is twelve rare-term negations on the 1M index, not `tiny_bench`,
  which times reader.hh only.

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
