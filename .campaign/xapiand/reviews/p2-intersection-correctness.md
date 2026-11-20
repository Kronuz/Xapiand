# p2-intersection review: correctness

Commit: prospect:ebd1c1395bcf011faea6598b9c5acc485d13b742
Verdict: CHANGES
Date: 2026-11-17

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- build-test 261117-034554-d8f846 at ebd1c13: 53 of 53 ctest pass, `intersection` included.
- Mutants at ebd1c13, caught: 0 (`<=`), 2 (tail not padded), 4, 5, 6 (rank) — 261117-034556-bfc0aa,
  -034603-7c65ba, -034609-858907, -034613-bb1349, -034616-5e4b22. Survived: 1 and 3 (R2).
- Identity 261117-034619-9464d1: verify exit 0, both dumps differ 0, 962 queries 0 mismatches.
- A/B 261117-034629-5eeca5 (1M, exclusive, 7 rounds) and CI 44076940308 as the journal states.

## What I checked myself

Scripts under `.scratch/review-p2-intersection-correctness/` in the home checkout.

- Code reading, Q1/Q2/Q4 (below), reader.hh at ebd1c13.
- 261117-053926-382162 (`precondition.sh` + `instrument.py`, light): traps in `find_in_block`
  unless at least two postings are decoded, `doc()` sits at `pos_`, `doc() < target <= buf_last()`,
  every slot past `buf_len_` holds `no_more_docs`, and the result is the first slot `>= target`
  past `pos_`; whole suite. Exit 0, 53 of 53 pass: no trap fired (`set -eu` and the script's assert stop the job if the edit fails to apply).
- 261117-053908-cd8c4d (`build.sh` + `run.sh` + `tiny_bench.cc`, strict, 7 interleaved rounds):
  leapfrog conjunctions over short lists, fresh iterators per query, the branch's reader.hh
  against main's (eef601d) in one build; `summarize.py` prints new/old and old/old.

| Case | main ns | branch ns | branch/main per round | A/A |
|---|---|---|---|---|
| tail 40 & tail 100 | 1494 | 3412 | 2.29 [2.05-2.48] | [0.87-1.19] |
| tail 10 & tail 120 | 1018 | 1573 | 1.51 [1.17-1.93] | [0.96-1.09] |
| tail 40 & 100 & 120 | 2125 | 3182 | 1.47 [1.31-1.83] | [0.85-1.09] |
| sparse 50 & 2000 (15 blocks) | 3554 | 5242 | 1.55 [1.17-1.78] | [0.94-1.41] |
| tail 100 & 2000 | 2904 | 4588 | 1.62 [1.04-3.47] | [0.83-1.75] |
| one posting & tail 100 | 783 | 764 | 1.08 [0.76-1.70] | [0.72-1.20] |
| one posting & 40 & 100 | 1168 | 1094 | 0.99 [0.66-1.67] | [0.74-1.51] |

  Same hits on both sides. It ran beside light holds (strict cores, shared caches), so the A/A is
  wide, but the first three rows sit above the A/A's maximum in every round.
- `loop.cc`/`loop.s`: the 128-slot loop does vectorize on this host (x86_64 i9-9980HK, SSE4.1
  `pmaxud`/`pcmpeqd`/`paddd`, no AVX2 without `-march`): about 100 instructions per call, against
  one to four predictable probes in main's `count_below` when the target is near `pos_`.

## Answers to the lead's questions

1. Padding. Every block `find_in_block` can see is full or padded: FOR/PFOR blocks decode all 128
   slots (`buf_len_ = format::block`, reader.hh:1162); a bitset block expands all 128 (`ords()`,
   reader.hh:1363); the tail pads `n..127` (reader.hh:1178), which also covers a tail decoded after
   a longer block (mutant 2 caught); a recycled iterator reruns the constructor. The single posting's
   fill (reader.hh:678) is unreachable code for this purpose (R2).
2. Precondition. Call site 1 (reader.hh:767): line 763 returns when `doc() >= target`, and
   `doc() == no_more_docs` implies `buf_len_ == 0` (load_next_block:1186, skip_to_block_containing
   clears it), so `doc() < target <= buf_last()` with `doc()` at `pos_`. Call site 2 (reader.hh:783):
   after a load (`pos_ = 0`, doc = slot 0) or a step (`++pos_`), guarded by `doc_ < target` and
   `target <= buf_last()`; a first advance after `skip_to_block_containing` always loads first
   (`buf_len_ = 0`). Holds on both paths; the trap probe above runs it over the suite.
3. `bits_rank_` (uint8_t): a valid block's rank before any word is at most 127 (128 bits, the last
   word holds the last one). A corrupt block can wrap the stored value, but load_bitset calls
   `corrupt()` before `bits_pending_` is set, so advance_bits never reads it. Words past `words`
   are stale but unread: advance_bits stops at the word holding `bits_last_`. No staleness.
4. Readers past `buf_len_`: none. `for_each_until` (reader.hh:926), `fill` (946) and
   `check_increasing` (1399) all stop at `buf_len_`; `ords()` only writes. No reader sees the
   padding where it used to see stale data.
5. Tiny lists: yes, a regression the 1M A/B cannot show (R1).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | yes | defect | The 128-slot compare costs about 100 vector instructions per call where main's `count_below` returned after one to four predictable probes for a target near `pos_`. Conjunctions over short lists and a sparse list against a blocked one run 1.5-2.3x slower (+0.5 to +1.9 µs a query); the 1M classes A/B (A3) only has long lists, where main's probes mispredicted. Short lists are the common case in the many-indexes workload. | 261117-053908-cd8c4d; reader.hh:1388-1393 | Keep the branch-free compare for far targets but return early for near ones, e.g. probe the next one to four slots from `pos_` first, and/or bound the compare at `buf_len_` rounded up to 16 for tails (padding makes that safe). Prove it with this probe (`run.sh` at the fix: no row above the A/A) plus the A3 A/B rerun at the fix commit. |
| R2 | no | gap | Mutants 1 (127 slots) and 3 (single posting not padded) survive at ebd1c13 (261117-034559-10e420, -034606-b7f744), yet the journal's A1 row cites them as fails-without evidence, and `campaign status` still tags A1 with the void 5227e00 mutant jobs (261117-025850-78b1d9 ...). Both mutants are equivalent: slot 127 is either `buf_last() >= target` or padding, and a one-posting block never reaches `find_in_block`, which needs `doc() < target <= buf_last()` in one block. | the two job logs; reader.hh:678, 1392 | Record A1 with the five caught mutants and name 1 and 3 as equivalent. Drop the single-posting fill (reader.hh:678, a 508-byte fill per single-posting iterator, no reader) or keep it as hygiene; owner's call. |
| R3 | no | nit | `bits_rank_` as uint8_t is load-bearing only while `format::block <= 256`; nothing states it. | reader.hh:1450 | `static_assert(format::block <= 256)` beside the member. |

## Not run

- The fix's effect at whole-query level on a small index (the classes A/B at 10k documents): the
  probe measures iterator work only; the end-to-end share depends on term lookup and scoring.
- arm64 (NEON) and AVX2 builds of the probe; the cost ratio may differ there, not the direction.
- An exclusive rerun of the probe; the strict run's rows that cross the A/A edges (sparse 50,
  tail 100 & 2000) are suggestive, not settled.
