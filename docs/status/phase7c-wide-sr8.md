> Status: HISTORY (2026-11-08): superseded by Xapiand's `.campaign/xapiand/` (`campaign map`); kept for its results and reasoning. Was: (2026-11-03) the developer's ledger for `fix/wide-search-sr8`: the SR8 follow-up, then the owner's ruling that indexes nest (nesting refusal removed, the pattern walk descends into index directories) and the lead's direction that patterns are path globs. Local branch from master `Xapiand:4f1cc9a94`, not pushed.

# phase7c wide search, SR8 follow-up and nested indexes (build)

Branch `fix/wide-search-sr8` in `Xapiand-wt/wide-sr8`. Sources: `docs/reviews/phase7c-wide-search-security.md` ("## Re-check"); mechanism `docs/mechanisms/wide-search.md`.

## Owner ruling (2026-11-03)

"Nested indexes is the heart of xapiand." Index paths nest: `twitter` and `twitter/user` can both exist, each a directory, the child inside the parent. The wide-search security work (SR2, SR9, SR10) wrongly refused nesting. It is undone here, and the real problem (SR2: creating `tenant` hid `tenant/a` from patterns because the walk stopped at an index directory) is fixed in the walk.

## State

Nesting refusal removed; the walk descends into index directories; patterns are globs over path segments with the automaton the lead asked for. SR8 (walk gate, queue cap, deadline) and the two test gaps are kept. Release ctest 37 of 37, docs E2E 666 of 666, `async_check` 0 owed, macOS ASan+UBSan clean, fuzz clean, every mutant dies. Local only, never pushed.

## Done

| Item | What | Commit |
|---|---|---|
| Remove the refusal | `check_new_index`, `check_created`, `OpenOptions::on_create`, the creation lock and rollback in `acquire`, the parent reads (SR10), the two renamed docs examples (restored), the two allowlist rows | Xapiand:708b6bfe3 |
| Fix the attack | `Walk::scan` reads a directory once, takes `strata` and `.__N` as the index itself (never listed, never entered; the data directory itself has no markers, so a top-level index may be called `strata`), lists the other directories as children, and goes into every directory a wanted pattern is still live in, index directories included | Xapiand:708b6bfe3 |
| Glob semantics | `*`, `?`, `[...]` inside one segment (never `/`), a segment `**` is any number of segments (none included, any number of them per pattern), `_all` is `**`. `index_pattern::Glob`: an automaton over the pattern's segments, a bitset of live positions carried down the walk, O(pattern segments) per entry, within a segment the two-pointer match. `*` used to cross `/` (changelog and api.md say so) | Xapiand:708b6bfe3 |
| Budget | every entry read, at any depth and inside index directories, counts against `--max-search-indexes`; the steps bound is unchanged | Xapiand:708b6bfe3 |
| Docs | api.md (rules, nested note, E2E for `*`, `**`, `?`, a class, `parent`/`parent/child` through `parent*`, `parent/*`, `parent/**`, `_all`, a count), wide-search.md (ruling, automaton, limits table), options.md, changelog, testing.md | Xapiand:c8df312fa |

SR8 (`WalkGate`, `--search-walk-queue`, `--search-walk-wait`) is untouched.

## Known edges (stated in wide-search.md)

- A child named `strata` below an index or a container (`x/strata`) is the engine's own name there: patterns do not list it (a name finds it; `collide.py`: `x` stays intact, `x/strata` answers by name). It cannot hide anyone else: the other children of `x` are still listed. `PUT /x/strata/d` under a container also makes `x` look like an index (it holds `strata`), so `x` may be listed with no data; searching it finds nothing.
- Capacity: an index costs its own entry plus the entries of its directory (`strata` or its shards, and its children) now, so `_all` reaches fewer indexes under the same `--max-search-indexes` (about 2 entries per one-shard index instead of 1). The lead may want the default raised; not done here.

## Proof (Release macOS `.scratch/build-rel`; scratch probes in `.scratch/nested/`)

- `ctest`: 37 of 37 (`test_index_pattern` 287 checks; `test_wide_search`, `test_pool`, `test_engine_strata` green).
- Docs E2E (`newman` 6.2.3, `.scratch/run_e2e_nested.sh`): 396 requests, 666 of 666 assertions, uncovered 0.
- `harness/async_check.sh --strict --owed --extern strata=<_deps/strata-src at 3f564dc5>`: 263 hits, allow 250, owed 0, unclassified 0; `test_gate_tools.py` OK. The `index_pattern.cc` row is 8 again (the SR9 rows are gone).
- ASan+UBSan (`.scratch/build-asan`, `detect_leaks=0`): test_index_pattern 287, test_engine_strata 9,735, test_pool 214, test_wide_search 22,301, no sanitizer line.
- Reviewer probes, run on the new head (copies in `.scratch/nested/`): `hide.py`: after `PUT /tenant/strata/d`, `_all` and `tenant/*` list `tenant/a` and `tenant/b`; `hide2.py` (1 and 5 shards): after `PUT /tenant/d`, `_all` lists `tenant`, `tenant/a`, `tenant/b` and `tenant*` lists `tenant`; `collide.py`: `x` total 5 before and after `PUT /x/strata/d` and after a restart.
- Differential oracle `diff_expand.py` (nested tree: 45 real indexes, 28 to 71 nested pairs, created in random order; random `*`, `?`, `[..]`, `**`, exclusions against a recursive segment matcher): seeds 1 to 11, about 240 patterns each, 0 mismatches (an earlier mismatch was the oracle's own regex on a reversed range).
- Fuzz (clang 22 libFuzzer, ASan+UBSan, light hold): `fuzz_index_pattern` (glob against a table-driven reference that tries every way, and the automaton: a prefix of a match stays live, accept equals match, steps bounded) 431,225 runs in 121 s, then 233,875 in 61 s with UTF-8 seeds; `fuzz_index_expand` (nested tree, oracle, entries bound) 383,544 runs in 151 s; no finding. Mutants: `?` consuming a byte aborts `fuzz_index_pattern` (`match(?, é) = 0`; it survived until UTF-8 seeds were added); the walk not descending into index directories aborts `fuzz_index_expand` (`mismatch`).
- SR1 with nesting (`nested_budget.py`, `quiet strict`, Release, a 100,441-entry tree of fake indexes inside indexes plus a chain 120 deep, cap 50,000): `_all`, `**`, `**/zz`, `**/h`, `**/g1*/**/h`, `**,-**`, `**/c/**/c/**/c/**/x` and 32 patterns with classes all answer 400 at the cap in 1.0 to 1.3 s; `*,-*` and `f1/g1*` read the top level or one directory only (0.00 and 0.01 s); `f1/**` 0.29 s, `c/**` 0.12 s. `bounded.py 20000 6 20020` (flat): worst 32 patterns 400 in 0.22 s, writes under 30 ms during walkers.

### Each change fails its test when reverted (mutants, one at a time, `.scratch/mutants_nested.py`)

| Mutant | Killed by |
|---|---|
| the walk does not go into index directories (the fix reverted) | `test_nested_indexes_are_listed`, `test_an_index_does_not_hide_the_ones_below_it`, `test_the_walk_goes_only_where_a_pattern_can_match`, `test_a_deep_tree_stays_within_the_budget` |
| `*` crosses `/` (a segment with a `*` acts as `**`) | `test_match`, `test_expand`, `test_nested_indexes_are_listed`, and 10 more |
| no pruning (`live` always true) | `test_the_walk_goes_only_where_a_pattern_can_match` (entry counts 7, 11, 37), `test_the_walk_is_bounded`, `test_one_walk_for_all_the_items` |
| `strata` and `.__N` entries of an index directory not counted | `test_the_walk_goes_only_where_a_pattern_can_match`, `test_the_entries_of_a_candidate_are_counted`, `test_a_deep_tree_stays_within_the_budget` and 4 more |
| `strata` taken for a child | `test_an_index_does_not_hide_the_ones_below_it`, `test_nested_indexes_are_listed` and 4 more |
| a top-level index called `strata` hidden | `test_an_index_does_not_hide_the_ones_below_it` |
| `?` is a byte, not a character | `test_match` |
| a `**` may not match no segment | `test_match`, `test_expand`, `test_nested_indexes_are_listed` and 5 more |
| exclusions ignored | `test_expand`, `test_nested_indexes_are_listed`, `test_the_limit` and 2 more |

New tests: `test_nested_indexes_are_listed`, `test_an_index_does_not_hide_the_ones_below_it`, `test_the_walk_goes_only_where_a_pattern_can_match`, `test_a_deep_tree_stays_within_the_budget`, `test_many_globstars_stay_linear` (`**/**/**/a` and others over a 2,000-segment path: steps within segments x pattern segments x 4), two `**` over a nested tree, `test_the_entries_of_a_candidate_are_counted` (rewritten: 41).

## Next

- The rechecker confirms the nested walk against the SR2 probes (copies in `.scratch/nested/`; the originals are in `Xapiand-wt/wide-search/.scratch/sec-patterns/`) and the glob semantics.
- Not run: `harness/wide_search_check.py check` (two servers; its nesting block and cap numbers were updated, not executed), TSan, Linux and FreeBSD (CI).
- Decision for the lead: the default of `--max-search-indexes` against the higher cost per index (see Known edges).

## Blocked on

Nothing.
