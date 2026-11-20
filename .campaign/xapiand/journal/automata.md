# automata: journal

State: waiting on jobs 261120-045742-f84874 261120-045743-bf9ae6 261120-045743-1ae13f 261120-045745-65164b
Quest: .campaign/xapiand/quests/automata.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Feature complete at prospect:cbe3fe7; A1 and A2 jobs queued at it.

## Next

1. When the four jobs pass: State "ready to land". Before landing (lead): push automata:a289a07 and
   termdict:e3f6f54 (prospect's pins resolve on GitHub only then; CI is red until), then land.

## Where the code is (three repositories, nothing pushed)

| Repository | Worktree | Branch | Head |
|---|---|---|---|
| Kronuz/automata | /Users/kronuz/code/Kronuz/automata-wt/xapiand/automata | quest/xapiand/automata | automata:a289a07 |
| Kronuz/termdict | /Users/kronuz/code/Kronuz/termdict-wt/xapiand/automata | quest/xapiand/automata | termdict:e3f6f54 |
| Kronuz/prospect | /Users/kronuz/code/Kronuz/prospect-wt/xapiand/automata | quest/xapiand/automata | prospect:cbe3fe7 |

Jobs reach the unpushed commits with `--ref AUTOMATA=/Users/kronuz/code/Kronuz/automata@SHA
--ref TERMDICT=/Users/kronuz/code/Kronuz/termdict@SHA` and `-DFETCHCONTENT_SOURCE_DIR_<NAME>=$JOB_REF_<NAME>`.
The quest has no gate checks (`campaign verify automata`: none), so the suite job is submitted by
hand with those refs.

## Blocked on

Nothing for the code. For the lead: the two library pushes above; Xapiand's mapping onto
`prospect::expand` (evaluation step 4), fst's deletion (step 5) and the branch's archive are other
repositories, outside this quest.

## Review fixes

- A1 (review automata-correctness): prospect:5d746f8. expand_test's check_budgets: prefix("") and * charge at least their terms' bytes, a budget below refuses; S1 (no per-term charge) is row "charge: ..." of expand_mutants.py, caught at expand_test.cc:348. Restored: expand_test passes (1,188 queries, 0 failures). N6: check_cancel_bound, a cancel at entry 5 stops within check_every 16. Not yet rerun as campaign jobs at the new head.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261120-045742-f84874 (expand_test), 261120-045743-bf9ae6 (automata_test, fuzz replay, termdict seek test), 261120-045743-1ae13f (ten mutants caught); all at prospect:cbe3fe7. Locally: expand_test 1,188 queries, 6,574 dead terms skipped, 0 failures; 10/10 mutants caught |
| A2 | 261120-045745-65164b (prospect suite, all ctest, at refs); CI after the library pushes |
| A3 | `docs/mechanisms/automata.md` |

Cancelled after the owner's 2026-11-20 change (old A2/A3 only): 261120-045652-723c68 (fuzz),
261120-045653-90093c (ASan+UBSan), 261120-045655-2fb958 (TSan).

## Result

- Kronuz/automata (automata:a289a07): strata's `automata.hh` verbatim in namespace `automata`, its
  brute-force tests, fuzz target (segment half now a termdict half) and corpus; new `walk()` over
  any seek/next/key cursor, `intersect_sorted` is that walk; `docs/design.md`.
- termdict (termdict:e3f6f54): `cursor::seek(from, visit)`, tested against `find_run`'s visits.
- prospect (prospect:cbe3fe7): `include/prospect/expand.hh` (intersect, expand, limits, liveness,
  pause/cancel/charge), `fielddict::reader::field_cursor`, `inverted_plugin::reader::field_cursor`,
  hashed `term_table` with `put()`; `test/expand_test.cc`, `test/expand_mutants.py`;
  `docs/mechanisms/automata.md`. Timings and the fuzz CPU-hour run after landing (owner).

## Rulings I made

- The library's contents (evaluation step 1) are built here: no other quest owns them and prospect
  needs them. Worktrees and branches follow the campaign's naming.
- D4's walk lives in the library as `automata::walk`; `intersect_sorted` is that walk over a sorted
  range. One walk, fuzzed where it lives; prospect supplies the per-field cursor.
- `pause()` is checked after every `check_every` entries, never before a walk's first key, so a
  resumed walk always advances. `expand` resumes a paused walk at once; callers that yield use
  `intersect`, which takes a `from` key (an addition to the contract's signature).
- `first`'s cutoff only bounds work; correctness comes from trimming past `max` (its first mutant
  survived for that reason and was replaced by one that changes answers).
- `term_table` owns its hashed keys (copies stay safe) and keeps its old add shape, so
  `stats_mutants.py` stays valid; its refusal mutant now names the index erase.
- The library's fuzz target bounds walks over corrupted blocks with `pause` (unsorted keys can send
  seeks back).
