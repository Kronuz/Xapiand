# automata: journal

State: working
Quest: .campaign/xapiand/quests/automata.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 2: termdict's `cursor::seek(from, visit)` (D5).

## Next

1. termdict worktree `termdict-wt/xapiand/automata`, branch `quest/xapiand/automata`: `seek(from,
   visit)`, tested against `find_run`'s visits.
2. prospect: per-field term cursor, `include/prospect/expand.hh` (D4 walk via `automata::walk`, D5
   infos, D6 limits and liveness, D8 hooks), `term_table` hashed index; brute-force tests (A1).
3. A2 jobs (fuzz CPU-hour, ASan/UBSan, TSan); A3 bench; A4 `docs/mechanisms/automata.md`.

## Where the code is (three repositories, nothing pushed)

| Repository | Worktree | Branch | Head |
|---|---|---|---|
| Kronuz/automata | /Users/kronuz/code/Kronuz/automata-wt/xapiand/automata | quest/xapiand/automata | automata:a289a07 |
| Kronuz/termdict | (step 1) | | |
| Kronuz/prospect | /Users/kronuz/code/Kronuz/prospect-wt/xapiand/automata | quest/xapiand/automata | |

Jobs reach the unpushed library commits with `--ref AUTOMATA=/Users/kronuz/code/Kronuz/automata@SHA`
and `-DFETCHCONTENT_SOURCE_DIR_AUTOMATA=$JOB_REF_AUTOMATA` (termdict the same).

## Blocked on

Nothing yet. For the lead: the library commits must be pushed (and CI green) before prospect's pins
resolve on GitHub; Xapiand's mapping (evaluation step 4), fst's deletion (step 5) and the branch's
archive are other repositories, outside this quest's (prospect).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <no evidence yet> |
| A2 | <no evidence yet> |
| A3 | <no evidence yet> |
| A4 | <no evidence yet> |

## Result

<pending>

## Rulings I made

- The library's contents (evaluation step 1) are built here: no other quest owns them and prospect
  needs them. Worktree and branch follow the campaign's naming in Kronuz/automata.
- D4's walk lives in the library as `automata::walk` over any cursor with `seek`/`next`/`key`, with
  `walk_options` (check_every, pause, resume_depth, charge); `intersect_sorted` is that walk over a
  sorted range. One walk, fuzzed where it lives; prospect supplies the per-field cursor.
- `pause()` is checked after every `check_every` entries, never before a walk's first key, so a
  resumed walk always advances.
- The library's fuzz target replaces the branch's segment half with a termdict dictionary (D2),
  bounding entries on corrupted blocks with `pause` (unsorted keys can send seeks back).
