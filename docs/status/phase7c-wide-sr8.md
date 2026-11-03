> Status: CURRENT (2026-11-03) the developer's ledger for the follow-up to the wide-search security re-check (SR8, SR9, SR10 and the two test gaps); local branch `fix/wide-search-sr8` from master `Xapiand:4f1cc9a94`, not pushed.

# phase7c wide search, SR8 follow-up (build)

Branch `fix/wide-search-sr8` in `Xapiand-wt/wide-sr8`. Source: `docs/reviews/phase7c-wide-search-security.md`, "## Re-check"; mechanism `docs/mechanisms/wide-search.md` (Limits table).

## State

SR8, SR9, SR10 and both test gaps are fixed and proved in Release (ctest 37 of 37, docs E2E 656 of 656, `async_check` 0 owed, fuzz rerun, timings under `quiet strict`). The ASan+UBSan run of the touched tests is recorded under "Proof" (see its row). Local only, never pushed; ready for the rechecker.

## Done criteria

- [x] SR8: a cap on queued pattern walks (429) and a wait deadline (503), both with `Retry-After: 1`, each a limits-table row with mechanism and reason.
- [x] SR9: two concurrent creates of nested names never both stand.
- [x] SR10: the ancestor reads of `check_new_index` are budgeted.
- [x] Test gaps: the entries read inside `holds_index` are pinned; `--max-search-indexes 0` is refused (tested).

## Done

| Finding | Fix | Commit |
|---|---|---|
| SR8 | `index_pattern::WalkGate` (`src/walk_gate.h`): the walk that runs plus `--search-walk-queue` (8) waiting; beyond that 429 at once; a waiter gives up at `--search-walk-wait` (10 s) with 503. `expand_paths` enters the gate; the view's catch writes the status and `Request::retry_after` becomes the header | Xapiand:5c895d94c |
| SR9 | the engine checks the index again where it comes to be: `acquire` makes the directory, then `OpenOptions::on_create` (`index_pattern::check_created`) under one process-wide mutex; a nested creation takes back only what it made (a parent only while empty) and answers 400 | Xapiand:5c895d94c, Xapiand:52de61a55 (a shard path is relative to the data directory), Xapiand:913c9db65 (async allowlist rows) |
| SR10 | the parents of a new index are read for at most `--max-search-indexes` entries in all (400 past it), the same budget as the walk; the descendant walk keeps its own | Xapiand:5c895d94c |
| Gaps | `test_the_entries_of_a_candidate_are_counted` (81 entries exactly: 1 + 40 + 40), `test_a_zero_cap_is_refused` (forked `parseOptions`) | Xapiand:5c895d94c, Xapiand:1ddabc9ad (the queue test no longer assumes the order of its waiters) |

New options: `--search-walk-queue`, `--search-walk-wait` (`options.md`, changelog). Limits table rows: walks waiting, wait for a walk, entries read to check a new index's parents.

## Proof (Release macOS, `.scratch/build-rel`, clang from Xcode)

- `ctest`: 37 of 37 (test_index_pattern 634 checks, six runs in a row green after the queue test was made order-independent; test_wide_search, test_pool and test_engine_strata green).
- Docs E2E (`newman` 6.2.3, `harness/e2e_capture.sh`): 386 requests, 656 of 656 assertions, 0 "Cannot create index" lines.
- `harness/async_check.sh --strict --owed --extern strata=<_deps/strata-src at 3f564dc5>`: 268 hits, allow 255, owed 0, unclassified 0; `test_gate_tools.py` OK. Two rows added to `async_allowlist.tsv` (the creation lock, the directory calls of the recheck).
- Fuzz (clang 22 libFuzzer, ASan+UBSan, light hold): `fuzz_index_expand` 272,770 runs in 121 s (it now also checks `check_created` against its brute-force oracle), `fuzz_index_pattern` 1,283,615 runs in 91 s, no finding. Mutant (the parents check skipped when a directory was just made) aborts `fuzz_index_expand`: `check_created(x.y/{[) = 0`.

### Each test fails with its fix reverted (mutants, one at a time, `.scratch/mutants.py`)

| Mutant (also the revert of the fix) | Killed by |
|---|---|
| gate: queue limit removed | `test_the_walk_queue_is_bounded` |
| gate: wait without the deadline | `test_a_walk_waits_for_the_deadline_at_most` |
| engine: `on_create` not called (SR9 revert) | `test_a_creation_that_nests_takes_its_directory_back`, `test_concurrent_creates_never_nest` |
| engine: creation lock removed | `test_concurrent_creates_never_nest` (one of a racing pair must stand; without the lock both were refused in 148 of 150 pairs) |
| engine: rollback removed | the same two tests |
| parents read without the budget (SR10 revert) | `test_the_parents_are_read_within_a_budget` |
| entries read inside `holds_index` not counted (the review's surviving mutant) | `test_the_entries_of_a_candidate_are_counted`, `test_the_parents_are_read_within_a_budget` |
| `--max-search-indexes` range check removed | `test_a_zero_cap_is_refused` |
| `check_created` ignores a relative shard path | `test_a_created_index_is_checked_again` |

Server wiring, by the probes: the first server run had the recheck inert (the node's shard paths are relative to its data directory, which the first `check_created` skipped): `nest2.py 60` then gave "both accepted for 60 pairs", the review's finding. With the fix, `nest2.py 60`: both accepted for 0 pairs. The limits switched off by flags (`--search-walk-queue 1000000 --search-walk-wait 3600`) give the old behavior in `starve4.py`.

### Timings (`quiet strict`, Release, `.scratch/wide-recheck/starve4.py`, `ancestor2.py`; walks are `*,-*` over 99,999 directories, the cap 100,020)

| Attackers held | Honest `live*` search | Plain search / PUT doc / PUT new index | Attackers |
|---|---|---|---|
| 4 | 3.54 s, 200 | 0.001 / 0.002 / 0.040 s | 4 x 200 |
| 6 | 5.46 s, 200 | 0.000 / 0.001 / 0.029 s | 6 x 200 |
| 40 | 429 at once | 0.000 / 0.003 / 0.003 s | 9 x 200, 31 x 429 (0.001 to 0.003 s) |
| 150 | 429 at once | 0.000 / 0.000 / 0.061 s | 9 x 200, 141 x 429 |
| 10, limits off (the head before) | 9.81 s, 200 | 0.001 / 0.001 / 0.003 s | 10 x 200 |

A walk alone takes 1.1 s warm here (6.8 s the first time). The review measured 56 s behind 40 walks and 240 s behind 150; with the limits the honest wait never passes `--search-walk-wait`: past 8 waiting the answer is a 429 in a millisecond. The 503 path was seen in a run under a light hold (host load high, so not a timing): of 10 attackers, one answered 503 at 10.0 s with `Retry-After: 1`; the unit test pins it at the configured wait. Writes and plain searches stayed under 3 ms in every row.

SR10 (`ancestor2.py`, 20,000 children in `big`): `PUT /big/new/d` takes 0.06 s with the cap at 40,000 (all 20,000 read) and answers 400 in 5 ms with the cap at 5,000 ("too many entries"); the cost is at most one cap of entries, as for the walk. A create at the top level takes 0.025 s in both.

## Next

- The rechecker confirms SR8 to SR10 and the two gaps (probes in `.scratch/wide-recheck/`: `starve4.py`, `nest2.py`, `ancestor2.py`, `mutants.py`).
- Linux ASan+UBSan and TSan of `test_index_pattern` on the shared lease (the gate uses threads and a mutex) before the branch lands.

## Blocked on

Nothing.
