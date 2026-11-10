# automata-evaluation: journal

State: ready for review
Quest: .campaign/xapiand/quests/automata-evaluation.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Evaluation complete in prospect `docs/design/automata.md` revision 2 (prospect:e8e5bd9): D46
folded in (Verdict, D1, D2, D4 to D6, D9, "Owner rulings (D46)" table, steps), "Numbers" filled
from the strict screen. Harness blocks byte-identical at prospect:39d142a (where the jobs ran),
429d0f0 and e8e5bd9 (sha1 d6450cf over the appendix blocks).

## Next

1. Review (lead's call), then land.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/design/automata.md`: "Port it; the owner ruled so in D46." (prospect:e8e5bd9; its Checks name the jobs, listed below) |
| A2 | D46 |

Jobs examined:
- 261109-173038-76ce62 (branch tests, Release, at strata:219ded0d via --ref): `automata` passed
  (8.9 s), `fuzz_automata_replay` passed, `expand` failed one check, `expand_test.cc:376`
  `CHECK(df_errors > 0 && other == 0)`, the known red from the branch's ledger. Exit 1 by design
  of the job (it reports the failure).
- 261109-173049-b37276 (same, ASan+UBSan): same result, no sanitizer report (automata 40.6 s,
  expand 88.6 s).
- 261109-174627-134722 (light, prospect:39d142a, `automata_eval check 2000`): 2,000 random term
  sets of valid 1- to 4-byte UTF-8, 16,000 queries (prefix 5,298, glob 5,325, edit distance 5,377),
  187,911 matches: the branch's automata over termdict with seeks, over restarted scans, and
  `intersect_sorted` all equal Xapiand's `pattern.cc` predicates over every term. Mutants caught:
  1 (k + 1), 2 (Xapiand's flag bits unmapped), 3 (seek one byte past the target).
- 261109-174628-59e91d (strict smoke, 14 queries, 1 run) and 261109-174630-31d66c (strict screen,
  70 queries, 5 runs, prospect:39d142a): four arms equal on every query; x/a class sums prefix
  1.18, glob ? 83.6, glob *x 1.97, fuzzy k1 p0 2,075, k1 p2 42.9, k2 p0 96.5, k2 p2 4.93; s/a 1.00
  to 1.06 non-fuzzy, 1.21 to 1.49 fuzzy; l/a 12.89 prefix, 0.90 to 1.04 elsewhere.
- Probes (unversioned, never evidence): 261109-173409-6a6ee2, 261109-173959-233147,
  261109-173439-fc8922, 261109-174331-9100a8.

## Result

prospect `docs/design/automata.md` revision 2: port per D46 (Kronuz/automata library, prospect
`expand.hh` with a per-field seekable cursor and infos decoded in the walk, termdict gains
`cursor::seek(from, visit)`, Xapiand keeps the query mapping and serves key-field edit distance,
dead terms skipped, fst's automaton concept and string-similarity's OSA plan dropped). Fuzzy
without a fixed prefix: 67 to 2,650 times faster than Xapiand's scan (1.0 to 1.5 s to 0.5 to 20
ms per query, x86_64 macOS, one segment of 3.96 M terms).

## Rulings I made

- The harness lives in the design doc's appendix and each job extracts it: the file set is
  `docs/design/automata.md` only, and a job must run committed code.
- Vocabulary: the 3,964,752-term Wikipedia vocabulary at strata `.scratch/phase2/vocab.tsv`
  (outside the checkout; the job verifies its SHA-256 090511d2...e8a before running).
- The bench runs at `strict` (smoke, then screen) and is labelled as a branch screen, not a
  milestone number.
- D4 settled on the screen: a seekable per-field cursor in prospect (restarted scans 1.21 to 1.49
  times on fuzzy); the key field restarts `keys_plugin`'s scan (no strata API change) until a
  profile shows it.
- D5 settled on the screen: infos decoded in the walk via an additive termdict visitor seek (a
  lookup per match costs 12.9 times the walk on prefixes, past the doc's one-tenth bar).
- The liveness probe (D46 Q2) is unmeasured here; the doc assigns it to the `automata` quest's A3.
- A1's row names the doc, not the jobs: it is an [artifact] criterion, the jobs ran at
  prospect:39d142a (cross-check, bench) and strata:219ded0d (branch tests), and the lead said not to
  rerun them. The doc's Checks cite each job; `campaign job rerun 261109-174627-134722
  261109-174628-59e91d 261109-174630-31d66c` makes head twins if the lead wants them.
