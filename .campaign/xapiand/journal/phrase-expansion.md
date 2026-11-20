# phrase-expansion: journal

State: working
Quest: .campaign/xapiand/quests/phrase-expansion.md (the lead's; never edit it)

## Doing

Feature built at prospect:8e4df48 (prospect repo, worktree prospect-wt/xapiand/phrase-expansion,
branch quest/xapiand/phrase-expansion, nothing pushed). Evidence jobs queued; local mutant smoke running.

## Next

1. Record job ids and the Evidence table; State "ready for review".

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
