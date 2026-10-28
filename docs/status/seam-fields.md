> Status: CURRENT (2026-10-28) X6 of strata's fields-and-keys design; nothing supersedes it.

# seam-fields (X6, phase 7a)

- Design: `strata/docs/design/fields-and-keys.md` revision 1 (section 7 X6, D2, D9, D12, D14; conditions C3, C4), review
  `strata/docs/reviews/fields-and-keys-design.md` (approve with conditions, re-check closed FK-R1 to FK-R6).
- Branch / worktree: `phase7a/seam-fields` at `~/code/Kronuz/Xapiand-wt/seam-fields`, from Xapiand master `5d7f3263d`;
  local only, not pushed, no CI run. Baseline (master) build for the comparisons: `Xapiand-wt/seam-fields-base`.
- State: implementing.
- Done criteria (from the lead's task; ticked with evidence only):
  - [ ] X1 `engine::Document` and the query nodes carry a field; the Xapian engine joins (field, term) into today's term.
  - [ ] X2 Every site of D12's counts migrated (`prefixed(`, `Query::term(`, `Query::wildcard(`, `Query::edit_distance(`,
        `add_term(`, `add_boolean_term(`, `add_posting(`); `git grep 'prefixed('` empty.
  - [ ] X3 SEARCH dumps and `:info` hashes equal master's on the Xapian engine; a term dump of the index equal.
  - [ ] X4 Docs E2E 576 of 576 (newman 6 at `~/.scratch/newman6`).
  - [ ] X5 The engine census unchanged (term shapes per document).
  - [ ] X6 `harness/seam_check.sh` green.
  - [ ] X7 The engine tests on both engines.
  - [ ] X8 ASan (Homebrew LLVM) on the engine tests.
  - [ ] X9 A mutant that maps a field to the wrong prefix fails a test.
  - [ ] X10 C4: the strata engine's interim mapping is named (`src/engine/strata/interim_terms.h`) and is the only place
        it joins a field and a term; C3: no new caller of the key adapter.
  - [ ] X11 `docs/mechanisms/seam-fields.md`.
- Done: (nothing committed yet)
- Doing: first build of the branch.
- Next: build, fix compile errors, run the engine tests, then the node comparisons.
- Blocked on: nothing.
- Findings folded in: none yet.
- Last verified: nothing yet.

## Log
- 2026-10-28: branch created from master `5d7f3263d`; seam, Xapian and strata natives, callers and tests edited; not built.
