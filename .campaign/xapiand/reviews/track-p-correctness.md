# track-p review: correctness

Commit: prospect:0a2af7dfe874e576fe679ac7d5b3c5a1b493f071 (quest/xapiand/track-p)
Verdict: APPROVE
Date: 2026-11-20

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The only code is P7 bit 1: `exclusion_probe` and `exclusion_scorer::collect_top_k`
(include/prospect/search.hh:343-380, :1151-1170), `term_scorer::take` (:511-517), the
`negations()` test (test/search_test.cc:58-116) and test/negation_mutants.py. `git diff
bd60f39 0a2af7d -- include src test CMakeLists.txt` is empty: everything after the landing commit is
docs and main's ci.yml.

## Evidence I relied on

- 261117-155032-ecb275 (Release build and full ctest at bd60f39, exit 0): 53/53. The code is
  identical at the head, so it stands for 0a2af7d; the head still needs its own build-test job for
  `campaign verify` (lead's step).
- 261117-155033-63b78b (engine from bd60f39 against main 0aad777, exit 0): `verify` 0 mismatches
  over 962 gate queries; `dump` counted and uncounted byte-identical (5,772 lines). The binary's
  SHA-1 (1dca4f9f) equals `.scratch/bin/head-2ce63ac`'s.
- 261117-155040-04a4e1, -155044-c780b6, -155047-253843 (mutants 0-2 at bd60f39): each log prints
  "caught" with search_test.cc:110, the value check.
- The P9 and P13 job logs named in Q3 below, read in full.

## What I checked myself

Scripts are in `.scratch/xapiand/review-track-p-correctness/` in the home checkout (`p7_probe.cc`
extends the earlier review probe of job 261117-192846-ccadf8).

- **Q1, the restart path across segments and deletions** (job 261120-033235-6cc5f2, at 0a2af7d,
  exit 0). Five segments: docid-ordered and permuted, interleaved docids, two with deletions, one
  tiny one. 19 negations: term chains, nested, and a phrase, OR, AND or exclusion as the excluded
  clause. Views are all five segments, reversed, each alone, and two interleaved ones. k runs from 1
  past every match, counted and not. Each result is compared by docid and exact score, rank by rank,
  against the generic loop: 3,952 runs, 0 failures. With every excluded clause wrapped to count
  `fresh()` calls, the group walk restarts at k = 10 and at k = 10^6 in every view that holds the
  30,000-document permuted segment (2 copies per clause: the walk's own and one restart). Segments
  with deletions run the generic loop (`live_scorer` has no `collect_top_k`,
  include/prospect/searcher.hh:55-97). A phrase excluded clause has no copy, so it falls back to
  the generic loop untouched (search.hh:1162). With the no-restart mutant (`if (d < last_)` to
  `if (false)`) the same probe fails 31 checks, the first in the all-segments view
  (`a - c`, 44,463 hits against 43,962). So the restart is load-bearing across segments, not only
  in search_test.
- **Q2, the threshold drop at ties** (read; probe above). `take` drops `s <= m`, with `m` from
  `min_competitive()`. That is exactly what `top_collector::collect` drops in both modes. Without
  `begin()`: `score <= min_` once full or seeded. With `begin()`: `min_` is `max(floor_, kth)` only
  when the whole segment's docids are above the k-th's, where `collect` also refuses a tie. Otherwise
  it is `nextafter(kth)` below (search.hh:317-324 against :237-248). So no hit `collect` would keep
  is dropped unprobed, and nothing else changes which hits reach the collector. The probe's dense
  query has 16 distinct scores over 27,354 matches, so the k-th ties at every k.
- **Q2, `+xylophone -music -film` `TOP_1000` on the 1M index** (job 261120-033239-8c925b, at 0a2af7d, exit 0). The landing parity (63b78b) compared the twelve short negations' counted dump only, and a
  counted nested negation has no exact count, so it runs the generic loop (search.hh:1776-1778,
  `c.counting` at :1154). That job never ran the bulk path on the nested short cases, this one
  included. This job builds the engine from the head and from main 0aad777 (`--ref`). It dumps the
  twelve short negations plus the gate's 19 negations, counted and uncounted, both schemes, k 10,
  100 and 1,000, and compares the dumps byte for byte. Result: 31 queries, 186 lines each way,
  counted and uncounted, identical to main. `+xylophone -music -film` returns its 28 matches at every
  k, with the same docids and exact scores in the same order.
- **Q4, mutant 3** (job 261120-033237-f21aea, at 0a2af7d, exit 0): search_test passes unmutated, and
  `e.doc() >= d` is caught at search_test.cc:107.
- **Q3**: every number in the P13 and P9 rerun sections checked against the logs of
  261119-225047-cd30ff, -225048-424d5f, -225047-032deb, -225047-25300c, -165452-b53291,
  -165445-56d539 (P13) and 261119-165438-2cf08a, -165439-7fcba3, 261117-092109-383ae4 (P9). See P1
  and the notes.

## Q3 in short

- **P13.** The rerun tables match their logs row for row: A/B, serve, indexing (+3.2%, medians
  27.72 and 28.60 s), decode (2.0-2.7x PFOR) and ctest 53/53. The A/B log prints the SHA-1s the doc
  names (main `bee621b9`, EF `e38694a6`). The serve and indexing scripts print only `file` for the
  same paths, but both binaries still hash to those values and date from Nov 17. "Removed" follows
  the plan's rule: two phrase rows lose (`phrase_2` `COUNT` 1.063 [1.010, 1.120] over an A/A top of
  1.049, and `phrase_3` `TOP_10` 1.044 [1.015, 1.073] over 1.013), and none wins. The 1.04% size
  saving needs every row inside the band, and two rows are not.
- **P9.** Both in-process runs print the same SHA-1s (main `614e4d1e`, ALPHA 0 `383a79c0`, ALPHA
  0.5 `df3c453b`). Every row the doc quotes from 2cf08a, 7fcba3 and 383ae4 matches. The rerun
  alone passes the plan's keep rule for ALPHA 0 on intersection 3 `TOP_100` (0.940
  [0.899, 0.983], A/A low 0.954) and `TOP_1000` (0.962 [0.931, 0.993], A/A low 0.964), with no row
  losing. In 383ae4 neither row cleared its A/A, and no row won at either ALPHA. "Removed" rests on
  the replication rule. That rule is not in P9's plan. It is the rule that removed P2's selector,
  and it predates this rerun (P2's verdict, prospect:f197b3d, Nov 17). The doc states the conflict
  openly, so the verdict is honest. Whether replication becomes part of every item's written rule
  is the owner's call (P2 below).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| P1 | no | gap | A5's "fails without" row cites 261119-165425-f30bdd (mutant 3), which proves nothing. It ran 3 s with an empty log in the reused `xapiand-track-p` slot, where `cmake -G Ninja` over the build-test's own `build/` most likely failed. `job-landed-mutant.sh` runs under `set -e`, so a configure failure exits 1, the same code as "caught", and the tool recorded it as a pass. The claim itself holds: 261120-033237-f21aea catches mutant 3 at the head. | f30bdd state (began 1795137914, ended 1795137915, log 0 bytes); f21aea log | Cite f21aea instead of f30bdd in the journal's A5 row. Mutant scripts should exit 2 on a failed configure or build, as `.scratch/xapiand/review-track-p-correctness/mutant.sh` does. |
| P2 | no | gap | P9's written keep rule ("an intersection row wins and no row loses") keeps bit 4 at ALPHA 0 on the rerun alone. The removal applies P2's replication rule, which P9's plan does not state. The doc says so. | p9-planner.md "Plan" and "Rerun"; 2cf08a b/a and c/a tables | Owner's call. One option keeps the verdict as written. Another states replication in each Track P item's plan from now on. A third pre-registers a third deciding run for bit 4 at ALPHA 0. |
| P3 | no | gap | The landing's short-negation parity (63b78b) is counted only, so the five nested short cases, `+xylophone -music -film` among them, never took the bulk path there. | parity.sh (short list, no `PROSPECT_DUMP_COUNT=0`); search.hh:1776-1778 | Settled: 261120-033239-8c925b, uncounted dumps identical to main. The doc's "parity clean" for the short list should name that job. |
| P4 | no | nit | `negations()` asserts the segment is permuted but not that `a` spans more than one group. If the writer's grouping changes, the restart path can go uncovered without a failure. Mutant 1 being caught shows it is covered today. | search_test.cc:81 | In the permuted case, `CHECK(prospect::term_scorer(inv, *inv.term(0, "a")).postings().groups() > 1)`. |
| P5 | no | nit | Mutant 3 is caught by the size check (search_test.cc:107), not the value check. That holds here because the mutant drops hits; the test checks values on every rank as well. | f21aea log | None needed. |
| P6 | no | nit | The serve and indexing rerun scripts (P9 job-engines.sh; P13 job-engines.sh and job-index.sh) log `file` but not `shasum`, so the binary's identity rests on the path and its Nov 17 mtime. | logs of 7fcba3, 424d5f, 032deb | Add `shasum` beside `file`. |

Notes, one line each:

- `+xylophone -music -film` `TOP_1000` stays a recorded speed loss (1.31x, about 6 µs) under the
  doc's own short-list rule. The verdict keeps bit 1 with that case named, and the selector that
  would remove it is unmeasured. That is the owner's call, not a correctness issue.
- The brief's "changes .github/workflows/ci.yml outside its file set" is main's 9603a13 brought in by
  the merge. `git diff origin/main...HEAD` does not touch it.
- `campaign status` shows every criterion's jobs as "ran other code". The head differs from their
  commits only in docs and ci.yml, so `campaign job rerun` or a build-test at the head is the lead's
  bookkeeping, not a code question.

## Not run

- Sanitizers, Linux, FreeBSD and arm64 (CI's). The probe is Release with asserts off; the P7 path
  has no asserts.
- No new timing. P7's A/B numbers, A14's gap rows and P2's entry were not re-derived from their logs.
- Excluded clauses that are NEAR, synonym or value-source scorers. The probe covers term, phrase,
  OR, AND and exclusion.
