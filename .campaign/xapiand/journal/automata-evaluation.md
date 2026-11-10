# automata-evaluation: journal

State: waiting on jobs 261109-174627-134722 261109-174628-59e91d 261109-174630-31d66c
Quest: .campaign/xapiand/quests/automata-evaluation.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Evaluation drafted in prospect `docs/design/automata.md` (prospect:39d142a), harness embedded in
its appendix and extracted by each job. Waiting for the cross-check and the strict bench.

## Next

1. Fill "Numbers" (and the D1 and Verdict figures) from 261109-174630-31d66c; decide D4 (cursor
   vs restarted scan) and D5 (lookup per match) from its s/a and l/a columns.
2. Commit, set State to ready for review; propose A2's PLAN row text under Blocked on (the lead
   writes PLAN.md).

## Blocked on

Nothing yet.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | pending |

Jobs examined:
- 261109-173038-76ce62 (branch tests, Release, at strata:219ded0d via --ref): `automata` passed
  (8.9 s), `fuzz_automata_replay` passed, `expand` failed one check, `expand_test.cc:376`
  `CHECK(df_errors > 0 && other == 0)`, the known red from the branch's ledger. Exit 1 by design
  of the job (it reports the failure).
- 261109-173049-b37276 (same, ASan+UBSan): same result, no sanitizer report (automata 40.6 s,
  expand 88.6 s).
- Probes (unversioned, never evidence): 261109-173409-6a6ee2, 261109-173959-233147,
  261109-173439-fc8922, 261109-174331-9100a8.

## Result

<When ready for review.>

## Rulings I made

- The harness lives in the design doc's appendix and each job extracts it: the file set is
  `docs/design/automata.md` only, and a job must run committed code.
- Vocabulary: the 3,964,752-term Wikipedia vocabulary at strata `.scratch/phase2/vocab.tsv`
  (outside the checkout; the job verifies its SHA-256 090511d2...e8a before running).
- The bench runs at `strict` (smoke, then screen) and is labelled as a branch screen, not a
  milestone number.
