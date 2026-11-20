# track-p: journal

State: ready for review
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

0. Session 2026-11-20: every rerun read and recorded at prospect:0792dae (docs only): P13 rerun
   section in `p13-positions.md` (verdict holds), P9 rerun section in `p9-planner.md` (removal
   holds by the replication rule, see Rulings), A14 gap table in `p7-exclusion.md`. idx-p13 removed
   (disk 96%); `.scratch/p13/index-variant.sh` rebuilds it (light, about 4 min).
1. Waiting on the lead's correctness reviewer; branch not pushed (lead's instruction this session).

## Review notes (for the correctness reviewer)

The only code on the branch is P7 bit 1 (`exclusion_scorer::collect_top_k` in
`include/prospect/search.hh`, prospect:bd60f39) and its tests; P2, P9 and P13 are removed and live
as patches in `docs/perf/`. Probe: (1) the restart path in a reordered segment's group walk, where
excluded clauses restart from fresh copies when groups step back, across multiple segments and
deletions (review job ccadf8 covered one multi-segment probe); (2) the probe-only-if-it-could-enter
condition at ties on the threshold and at k above the match count (`+xylophone -music -film`
`TOP_1000`); (3) that the measured P13 and P9 reruns ran the Nov 17 binaries named by SHA-1 in each
doc, not rebuilds at the head (the head's only code change is off their paths); (4) the P9 verdict:
the head rerun alone passes the keep rule on two intersection 3 rows, and the removal rests on the
replication rule.

## Blocked on

Nothing. The lead's split (quest Decisions, 2026-11-19) moved A2-A4, A6, A8, A9, A11-A13 to
track-p-research; this quest lands A1, A5, A7, A10, A14.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | P2 kept in part then selector removed, `docs/perf/p2-intersection.md`: 261117-155032-ecb275 (passed at bd60f39769e0; docs-only since); fails without: 261117-121859-d28384, 261117-121924-2b4aee, 261117-121932-c981c4, 261117-121941-230f53, 261117-121958-98b81c, 261117-122016-ef86f3, 261117-122028-16a1f0, 261117-122035-212d65, 261117-123155-1419b0 |
| A5 | P7 bit 1 kept, `docs/perf/p7-exclusion.md`: 261117-155032-ecb275, 261117-155033-63b78b, 261117-155036-9088e8 (passed at bd60f39769e0; docs-only since); fails without: 261117-141718-c0035a, 261117-141721-81b492, 261117-141725-aa9027, 261117-155040-04a4e1, 261117-155044-c780b6, 261117-155047-253843, 261120-033237-f21aea (replaces f30bdd, whose build never ran: review P1) |
| A7 | P9 removed, `docs/perf/p9-planner.md`: 261119-165438-2cf08a, 261119-165439-7fcba3, 261119-165432-b1efdc (passed at 2ce63ac08bac; docs-only since) |
| A10 | P13 removed, `docs/perf/p13-positions.md`: 261119-165452-b53291, 261119-165445-56d539 (passed at 2ce63ac08bac), 261119-225047-cd30ff, 261119-225048-424d5f, 261119-225047-032deb (passed at 162b5d0c6a7d); docs-only since |
| A14 | gap rows at head, `docs/perf/p7-exclusion.md` "Gap rows at the quest's head": 261119-165524-86e55d (passed at 2ce63ac08bac; docs-only since) |

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

Reruns at the head (prospect:0792dae records them). A10: EF per block 0.96-1.08 of main on phrase
rows, two lose (`phrase_2` `COUNT` 1.063, `phrase_3` `TOP_10` 1.044, the latter in both runs), none
wins; serve 0.96-1.13; indexing +3.2%; decode 2.0-2.7x PFOR's; ctest 53/53. A7: bit 4 at ALPHA 0
clears the A/A on intersection 3 `TOP_100` (0.940) and `TOP_1000` (0.962), not replicated from the
first run; removed. A14: head vs Tantivy, negated 3 `TOP_10` 0.969 -> 0.466, `TOP_100` 1.157 ->
0.747; still losing: phrases 1.15-1.28, intersection 2 `TOP_1000`/`TOP_100_COUNT` 1.15/1.26,
intersection_union 2 `TOP_1000` 1.19, negated `TOP_1000` 1.10-1.15 and negated 3 counted rows;
head beats Lucene on every row but one crossing 1.0.

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
- A7: the head rerun (2cf08a) alone passes the keep rule for bit 4 at ALPHA 0 on two intersection 3
  rows; neither won in the first deciding run (383ae4), so by the replication rule applied to P2's
  selector (a row must clear the A/A in both deciding runs) P9 stays removed. Not a third run:
  picking a verdict by more draws of a 4-6% effect on 83 queries is not a decision rule.
- A7, A10: the reruns re-measure the Nov 17 builds (SHA-1 in each doc); the head's only code
  change (P7, `exclusion_scorer`) is off both paths, so no rebuild.
