# phrase-expansion: journal

State: waiting on jobs 261120-145150-ba6eac (build-test) 261120-145151-fb3e16 (ASan: scorer_budget, expansion_bounds, phrase_expansion) 261120-145153-531c56 (TSan: scorer_budget) at prospect:dea0ffb. CI at 15d2830 failed scorer_budget_test: ASan alloc-dealloc-mismatch (stable_sort's buffer freed by the test's replaced delete) and TSan multiple definition of operator new; fixed by not replacing the allocator under a sanitizer (the test then checks only that bytes are offered)
Quest: .campaign/xapiand/quests/phrase-expansion.md (the lead's; never edit it)

## Doing

Feature built at prospect:a69a08d (prospect repo, worktree prospect-wt/xapiand/phrase-expansion,
branch quest/xapiand/phrase-expansion, nothing pushed). Evidence jobs queued at it. Local mutant smoke (6 of 19) found one survivor (an empty query budget
read as none, only its early stop differs); a69a08d adds the test that catches it (checked locally).

## Mutant survivor (a69a08d, job 261120-085334-deefb6)

Survivor: "the walk's charge is not offered the glob's matcher" (glob built without walk.charge).
Real gap: tests refused only a prefix word with a throwing charge, and a glob's matcher charge
(automata.hh glob ctor) fires at construction, so a wildcard or fuzzy word matching no term is
charged only through it. prospect:1f7ddaf makes expansion_bounds_test refuse a prefix, a
wildcard("zz*yy") and a similar("zzzz", 1) word under a throwing charge. Resubmitted at 1f7ddaf.

Why 1f7ddaf did not kill it (job 261120-095314-390e6c survived, and msg from the coordinator): a
throwing charge fires on the walk's own matcher (walk_of passes o.charge to the matcher), so
dropping the charge from the glob constructor changes only the bytes counted, never a refusal. A
self-calibrated budget (total measured on the same build) does not see it either. Verified locally
under quiet light in a scratch clone (mutant applied to the copy, deleted after): the old test gave
0 failures on the mutant. prospect:facbd3e adds an absolute check: a glob's or fuzzy word's total
charge must exceed what its automaton constructor charges alone (and the budget one below the
total refuses). Mutant build: expansion_bounds_test 1 failure (that check); unmutated: 0 failures.

## Review CHANGES (phrase-expansion-correctness.md, at facbd3e), folded in at prospect:692e42e

- F1 (blocking, fixed): `scorer()` offers each segment's scorer bytes to the byte budget before
  allocating (per present slot term: the slot's term_info copy, the cursor's postings iterator and
  positions reader, the slot scorer's iterator and copies, +256 for vector growth and rounding),
  summed across segments. Test: expansion_bounds_test "The byte budget covers the scorers too"
  (a budget admitting the expansion but not one scorer refuses; exact at the total). Mutant "scorer:
  a segment's scorer bytes are not charged" (20 mutants now). New `test/scorer_budget_test.cc`
  (the reviewer's P1 as a test): charge within 10% of the heap, at 1 and 16 segments, both scorings;
  first run 0.933x to 0.945x, +256/term brings it to about 1.0x (job below). The note states what
  the budget covers (walks, scorer construction; not what a search decodes afterwards).
  The mutant is proved by the mutant job (the script is the "fails without" run), not a separate
  commit with the charge removed.
- F2 (fixed): under the query's bound a word expands as `error` at max(room, 1), whatever its type;
  test: first and most_frequent walk exactly the entries `error` does (mutant "a word the query
  refuses keeps its first or most_frequent type").
- F3: inherited from automata N1 (sliced matcher recharged); over-counting only; nothing here.
- F4: stated in the note: the bound counts the snapshot's distinct live terms, not a segment's.
- F5: NEAR needs distinct positions: kept as is, said in the note; for the owner (Lucene's unordered
  intervals overlap), never blocking.
- F6: identical words expand and charge once each, linear (P3); optional, left.
- F7: note reworded: `expand` resumes a paused walk at once; pause() only bounds check granularity.

## Next

None: ready for review. The lead reviews and lands.

## Where the code is

`include/prospect/phrase_expand.hh` (`phrase_word`, `expanded_phrase`, `phrase_options`,
`phrase_expansion_error`); `test/phrase_expansion_fixture.hh` (world and brute-force evaluation),
`test/phrase_expansion_test.cc` (A1), `test/expansion_bounds_test.cc` (A2),
`test/phrase_expansion_mutants.py`, `fuzz/fuzz_phrase.cc` + `fuzz/corpus/phrase`,
`docs/mechanisms/phrase-expansion.md` (A3); CMakeLists.txt wires the tests and the fuzz replay.

## Blocked on

Nothing.

## Rulings I made

- The four tasks (phrase, near, bounds, note) are one change: NEAR is `multi_phrase_scorer`'s window,
  so one `expanded_phrase` serves both; no separate NEAR code.
- `multi_phrase_scorer` (landed) is the matcher; this adds only the expansion into its slots and the
  bounds. No new scorer, no position logic.
- Bounds: a per-word `expansion_limit` (Xapian's three types) and a per-query `max_expansions`; both
  default to none (the walk's byte budget is the memory bound), literal words are not counted, past a
  bound `phrase_expansion_error` names the word and which bound; the tighter is named, the word's on a
  tie. `first`/`most_frequent` over the query's room are handed one term more than is left, so the
  overflow is seen and refused.
- Scoring: one weight per slot for the whole snapshot (not per segment), SYNONYM by default
  (ARCHITECTURE: expansions combine as SYNONYM), OR as an option; `phrase_options::weigh` overrides.
- The key field takes no phrase (`std::invalid_argument`): its terms have no positions.
- The oracle reads the snapshot only for dfs (deleted documents counted, as everywhere), the model for
  everything else. A literal word whose term has only dead documents may or may not stay in `terms()`
  (the evaluation accepts either); no document can match it.
- A bounded walk's early stop is asserted as monotone and well below the unbounded walk (a segment's
  first entries can be dead terms), not as "stops at the fourth entry".
- No timing and no `docs/perf` entry: the quest's criteria are functional; the expansion class's
  speed rows are Phase 5's (parked), as the quest's "When done" says.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261120-134019-bd8050 (58 of 58 tests: phrase_expansion, expansion_bounds, scorer_budget, fuzz_phrase_replay and the suite) and mutants 261120-134020-aaa9c5 (20 of 20 killed, 0 survivors), at prospect:15d2830 |
| A2 | the same two jobs; scorer budget probe 261120-130859-359099 (charge 0.965-0.968x of the scorers' heap at 1 and 16 segments, within 10%) |
| A3 | `docs/mechanisms/phrase-expansion.md` (states what the byte budget covers) |
| Review F1 (scorer bytes charged) | expansion_bounds_test "The byte budget covers the scorers too" and `scorer_budget_test`, in 261120-134019-bd8050 and 261120-130859-359099; mutant "a segment's scorer bytes are not charged" killed in 261120-134020-aaa9c5 |
| Review F2 (a refused most_frequent word stops early) | expansion_bounds_test entry counts equal across limit types, in 261120-134019-bd8050; mutant "a word the query refuses keeps its first or most_frequent type" killed in 261120-134020-aaa9c5 |

Superseded jobs: 261120-122152-e989f4, 261120-122153-0a987b, 261120-130855-0a1e98, 261120-130856-f2bab7 and earlier.

## Result

- `include/prospect/phrase_expand.hh`: `phrase_word` (literal, prefix, wildcard, fuzzy, own limit),
  `expanded_phrase` (expands once over the snapshot through `expand`, one weight per slot, a
  `multi_phrase_scorer` per segment, `builder()` for `search`), `phrase_options` (`max_expansions`,
  walk options, slot scoring, weights), `phrase_expansion_error`.
- Tests: `test/phrase_expansion_fixture.hh` (world, oracle), `phrase_expansion_test.cc`,
  `expansion_bounds_test.cc`, `phrase_expansion_mutants.py` (19), `fuzz/fuzz_phrase.cc` and its corpus;
  CMakeLists.txt wires them. Note: `docs/mechanisms/phrase-expansion.md`.
- For the Xapiand quest: phrase wildcards and `_synonym` over expanded words map onto `phrase_word`
  and `expanded_phrase`; the one-CPU-hour fuzz of `fuzz_phrase` and the ASan/UBSan builds are the
  quest's "When done" (lead).
