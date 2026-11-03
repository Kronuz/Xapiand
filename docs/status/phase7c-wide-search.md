> Status: CURRENT (2026-11-03) the wide-search developer's ledger (phase 7c: bounded-concurrency searches in batches); local branch `phase7c/wide-search` from master `Xapiand:11e3d82a4`, not pushed.

# phase7c wide search (build): a search walks its indexes in batches

Branch `phase7c/wide-search` in `Xapiand-wt/wide-search`. Source: `strata/docs/status/road-to-release.md`, "Wide searches"; mechanism `docs/mechanisms/wide-search.md`.

## State

Built and proved; the four findings of the first review round are fixed, and the security review's findings (SR1 to SR7, below) are fixed or answered. Master (Linux gate fixes, X-POOL step 1, strata pin 3f564dc5) is merged in (Xapiand:9201227c8). Local only (never pushed); ready for the security re-check. Code head Xapiand:2533b89c3; the fix round is Xapiand:b6b454d9a to Xapiand:2533b89c3, then the ledger.

## Done criteria

- [x] `--max-indexes-per-search` removed (code, test, harness, docs, changelog); `--concurrent-search-indexes` (K) and `--max-search-indexes` (100,000, 400 above) added with `options.md`, `reader-views.md`, `wide-search.md` and a changelog bullet. Old status and review files keep the old name as history.
- [x] A search walks its endpoints in batches of whole indexes, at most K shards, merging top-k as each batch ends, behind one seam (`engine::BatchRunner`; `DatabaseHandler::search_batch` is the one function that pins a batch).
- [x] Identity test `tests/test_wide_search.cc` (22,301 checks): the same hits, ids, order, ranks, weights, percents, totals and spy results for 1, K-1, K, K+1 and 3K+2 shards at K = 4, and for K = 1, K-1, K, K+1, 3K+2 and one batch over the same 3K+2 shards, against one search of all and against the engine's own search; relevance queries (term, rare term, OR, AND, wildcard by prefix with a SYNONYM and an OR combiner, synonym) next to match_all, keyed sorts with ties, both docid orders, windows, collapse. Non-vacuity: stored values reorder and drop hits; at least 5 relevance queries weigh differently when the indexes are read as one; every case at 3K+2 shards finds something.
- [x] A document weighs as in a search of its own index alone (`test_a_weight_is_that_of_the_documents_index`, 5 queries, up to 126 documents each).
- [x] Hit ids are 64 bits; tests at the old 32-bit edge, one batch and two (`test_an_id_past_32_bits_is_exact`), and the decode (`test_a_hit_id_decodes_past_32_bits`).
- [x] Patterns: `tests/test_index_pattern.cc` (glob, expansion over a directory tree, the limit), `tests/fuzz_index_pattern.cc` (the glob against a table reference: 1.25 million runs in 90 s under ASan+UBSan, no finding), E2E examples in `api.md`.
- [x] `harness/wide_search_check.py check`: two servers (all in one batch, K = 4) agree on sorted and relevance searches (with weights and ids), by name list and by pattern; the cap (`--max-search-indexes 7`) through a list, `_all`, a prefix and an exclusion. `gate` searches `/w*/`.

## Proof (fix round, head Xapiand:6ae5e1d5b; Release in `.scratch/build-rel`; Linux ASan+UBSan on the shared lease, clang 20, commit Xapiand:9c8da59a0, which differs from the head in harness, docs and a one-line `lstat` change only)

| | Release (macOS) | ASan+UBSan (Linux, `detect_leaks=0`) |
|---|---|---|
| ctest | 36 of 36 | 36 of 36 (`test_text` needs `allocator_may_return_null=1`: it decodes a msgpack array header that asks for 128 GiB, and ASan refuses the allocation; unrelated to this change) |
| docs E2E (newman 6.2.3) | 386 requests, 656 of 656 assertions | 656 of 656, no sanitizer line in the node log |
| `wide_search_check.py check` | PASS | PASS, no sanitizer line |

- `async_check.sh --strict --owed --extern strata=3f564dc5`: 255 hits, allow 242, owed 0, compiled_out 13, unclassified 0, exit 0 (the pattern walk is classified: a query chore); `test_gate_tools.py` OK.
- Mutants and reverts (each restored from git; the test or harness named fails):

| fix | mutant (also the revert) | killed by |
|---|---|---|
| 2 per-index weights | `strata::run` builds one scope for all the shards (the old per-batch statistics) | `test_wide_search`: 1,931 weight checks, 1,551 id checks, 312 oracle checks; the harness (every relevance and sorted search over 5 and 14 indexes) |
| 2 whole indexes per batch | `batch_end` ignores the index boundaries | `test_a_batch_holds_whole_indexes`, `test_the_hits_do_not_depend_on_the_batch_size` (522 weight checks) |
| 3 64-bit ids | the merge narrows the id to 32 bits | `test_an_id_past_32_bits_is_exact` |
| 3 decode | `split_hit_id` truncates | `test_a_hit_id_decodes_past_32_bits` |
| 1 expansion | the view does not expand | docs E2E (5 assertions of "Multiple Indexes") and the harness (patterns, caps through `_all`) |
| 1 closing slash | the last name keeps its `/` | the same E2E assertions and harness checks (found by the first E2E run: a pattern ending a path never matched) |
| 1 walk | no descent into containers; exclusions ignored; no limit in the walk | `test_expand`, `test_the_limit` (9, 4 and 2 checks) |
| 4 cap reachable | (the `_all` / prefix / exclusion cap rows of the harness) | the no-expansion mutant above |

- Fuzz: one target (the glob). The directory listing reads names, not bytes of a format, and takes no decoder of its own.
- Earlier mutants (merge tie-break, collapse window, window truncation, missing-shard renumbering, and the unbatched revert of Xapiand:dfbcc88e9) are unchanged; `test_wide_search` still holds those checks.

## Fix round (the lead's four findings)

1. **Wildcards and `_all` expand.** `search_view` and `count_view` expand `_all`, `*` patterns and `-x` exclusions against the node's directories (`src/index_pattern.{h,cc}`), in a `query` chore of the work pool (the reactor never lists a directory), and count the names against `--max-search-indexes` as the walk goes (400 above it, naming the option). An index is a directory holding `strata` or a shard `.__N`; `.`-led names are not listed; a pattern that matches nothing answers 200 with no hits. The resolved endpoints are sorted once (`Endpoints::add` inserted one by one, quadratic for a wide list). E2E examples with assertions are in `reference-guide/api.md`, "Multiple Indexes" (list, wildcard, inside a name, exclusion, `_all`, `COUNT`, no match); `harness/wide_search_check.py` checks patterns against name lists and the cap through `_all`.
2. **Results do not depend on K.** prospect can give per-index weights without a change: `term_table` takes any span of segments, so `strata::run` builds one term table and plan per index (`SearchRequest::index_of` names the index of each shard; the handler groups the endpoints by `unsharded_path`) and each segment builds with its index's. `search_batched` takes whole indexes per batch (an index wider than K is a batch alone). The weight of a document is that of a search of its own index alone (`test_a_weight_is_that_of_the_documents_index`), and the hits, order, weights, percents, totals and spy results are the same for K = 1, K-1, K, K+1, 3K+2 and one batch (`test_the_hits_do_not_depend_on_the_batch_size`), for relevance queries (term, OR, AND, wildcard, synonym), keyed sorts, collapse, both docid orders and windows. A single search over several indexes now weighs per index too, as Elasticsearch does by default.
3. **64-bit hit ids.** `engine::hitid` (uint64) is the id across the shards: `Hit::did`, the merge, `Document` and `DatabaseHandler::get_document`; `split_hit_id` decodes it (and refuses a document no shard can hold). A shard's own docid stays 32 bits. `test_an_id_past_32_bits_is_exact` and `test_a_hit_id_decodes_past_32_bits` sit on the old edge (42,949 documents of each of 100,000 shards).
4. **A long index list.** Decided: patterns, not a body list. A URL holds 64 KiB (about 7,000 six-character names) but `_all` or `wi*` is a few bytes, so the 100,000 cap is reachable by a request: `_all` over 100,000 indexes. A body list would be a second way to say what the path says (with a rule for both), and the Elasticsearch API this follows takes indices in the path and as patterns only. The harness proves the cap through `_all`, `w00*` and an exclusion list at `--max-search-indexes 7`; the gate now searches `/w*/`.

**K default, against the design's reader cap.** `readers_per_index` (16 searches per reactor, `manager.cc`) bounds the readers of one index; a batch takes one record per index it holds, so K does not enter it. K bounds the indexes pinned: one wide search per loop holds at most pool / loops, so the loops together hold the pool's budget (sized from the file-descriptor limit). More wide searches on one loop than one add their batches; the pool is soft (a used entry is never evicted, an overflowing idle one goes after 60 s), so that is a larger working set and shows in `xapiand_search_pinned_indexes_peak`, not a failure. The default stands; written in `wide-search.md`.

## Security fix round (docs/reviews/phase7c-wide-search-security.md, REJECT on SR1)

Master (Linux gate fixes, X-POOL step 1) is merged in Xapiand:9201227c8; `search_batch` pins each shard (`co_await shards.back().pin()`) as master's `lock_shard` sites do. The probes of the review are in `.scratch/sec-patterns/` (kept; `bounded.py` and `corrupt.py` are new, `rerun.sh` reruns the others).

| Finding | Fix | Evidence |
|---|---|---|
| SR1 the walk's work | one walk finds the names of every pattern (each entry tested against all of them), read entry by entry; every entry read counts against `--max-search-indexes` (matching or not; inside an index directory too), 400 past it; matching steps are counted too (4,096 per entry the cap allows), because 32 backtracking patterns on a 100,000-entry tree held a thread 48 s inside the entry budget; at most 32 patterns (wildcards and exclusions together); the walk calls the request's cancel token (`Request::cancel`) between entries; one walk at a time per node (a cancellable FIFO `task::async_mutex`), so walks hold one work-pool worker | `test_the_walk_is_bounded`, `test_the_matching_is_bounded`, `test_one_walk_for_all_the_items`, `test_the_patterns_are_bounded`, `test_the_walk_can_be_cancelled`; `bounded.py` (below); limits table in `docs/mechanisms/wide-search.md` |
| SR2 hiding | `index_pattern::check_new_index`, run in a query chore before the settings of a new index are saved: 400 when the path is below an index, above one, or has a name below its first that the layout keeps (`strata`, `.__N`) | `test_index_paths_do_not_nest`; `hide.py`, `hide2.py`, `collide.py` now answer 400 for the hiding PUT and list the same indexes; `wide_search_check.py` nesting rows; the docs E2E needed two renames (below) |
| SR4 duplicates | equal items merged before counting; the walk's matches and the plain names are counted apart, so a name in both counts once | `test_equal_items_count_once` |
| SR5 cap 0 | `--max-search-indexes` must be at least 1 (`CLI::Range`); `check_search_fanout` has no 0 case | `test_pool` (`check_search_fanout(1, 0)` is 400); `--max-search-indexes 0` is refused at start |
| SR6 | api.md says an unknown name no longer protects an index (`,`, `*`, `_all`, `-`; `:count` on a prefix tells whether names exist) | docs |
| SR7 | no code change: a really damaged index is not skipped. `corrupt.py` damages the manifest of a committed index: `bad` by name, `_all`, `bad,good` and `:count` all answer 500 `manifest: magic`; `good` by name 200. `search_batch` is master's code (it skips only `not_found`), so by name master behaves the same. The review's probe planted files strata does not read as an index, which strata reports as "no index was committed here" (`index_not_created`, `not_found`), the same as a missing name | `corrupt.py`, `corrupt.out` |
| SR3 | left to the 10,000-index gate run, as asked: yield in the per-name loop of `resolve_index_endpoints` if `stall.py` against real indexes shows a loop stall | open |

**Measured** (macOS, Release, `quiet strict`, `bounded.py 99999 40 100020`: 99,999 fake index directories of 157-byte names plus 5 entries, cap 100,020; before is the review's numbers or this probe on the build before the fix named):

| request | before | after |
|---|---|---|
| 1000 items of `*` plus `-*`, 50,000 dirs (review) | 518.7 s | 1 item merged: 1.07 s over 99,999 dirs (warm; the first walk, cold, 3.5 s) |
| 32 backtracking patterns, 99,999 dirs (this probe, before the step budget) | 48 s | 400 in 0.47 to 0.78 s |
| 33 patterns | served | 400 in 1 ms |
| a PUT to an open index while 40 worst-case walks are in flight | 36 s (review, 40 requests of 40 items); 1.3 s (this probe, entry and step budgets without the one-walk rule) | 1 ms (control with no walkers 1 ms); a PUT creating an index 3 ms (control 27 ms) |
| `live*` search while 40 worst-case walks are queued | | 18 s (the queue: one walk at a time, 0.5 s each); the same search right after the 40 clients left: 0.15 s (they cancelled) |

The consequence of one walk at a time: pattern searches queue behind each other; plain searches, writes and opens do not wait for them. A data directory with more entries than `--max-search-indexes` in the directories a pattern must read cannot be walked by a wildcard (a flat directory of a million indexes needs the cap raised to match, or names); that is the cost of making the cap the bound of the work. Raising the cap raises the worst walk linearly (about 1 s per 100,000 entries, warm).

**Data made before this round can nest** (nothing refused `PUT /a/b/d` beside an index `a`, and the docs E2E did exactly that). A pattern does not find the inner index, and searching it by name works. The check runs on creation only; no startup scan reports existing nested pairs. Decision for the lead: a one-off report (a walk that lists index directories holding a non-layout subdirectory) if there is data to worry about.

**Docs E2E**: the examples nested `test` beside `test/*` (global terms) and `test/namespace` beside `test/namespace/*`; renamed to `test_global` and `test/namespace_root` (path edits only, 656 of 656 assertions as before).

### Proof (head `Xapiand:2533b89c3` for code; docs and ledger commits follow)

- Release ctest 37 of 37; docs E2E (`newman 6.2.3`) 386 requests, 656 of 656; `wide_search_check.py check` PASS (cap rows, nesting rows); identity test `tests/test_wide_search.cc` unchanged (`git diff 088eab99e -- tests/test_wide_search.cc` is empty).
- `async_check.sh --strict --owed --extern strata=3f564dc5`: 254 hits, allow 241, owed 0, compiled_out 13, unclassified 0 (the `index_pattern.cc` row is 9 syscall sites: the walk and the nesting check, both query chores); `test_gate_tools.py` OK.
- macOS ASan+UBSan (Homebrew LLVM 22, `.scratch/build-asan`, `detect_leaks=0:allocator_may_return_null=1`): `test_index_pattern` 108, `test_endpoint` 144, `test_pool` 214, `test_text` 24,526 checks pass; the docs E2E 656 of 656 and `wide_search_check.py check` PASS on the sanitized server, no sanitizer line in the node logs. **Linux ASan+UBSan was not run** (see Blocked on).
- Fuzz: `fuzz_index_pattern` (the glob) 1,196,998 runs in 91 s; the new `fuzz_index_expand` (expand and check_new_index against a brute-force oracle, entries read included) 257,842 runs in 121 s; ASan+UBSan, no finding. Its mutant (`visits_` limit raised to 100,000) aborts it.
- Mutants of `src/index_pattern.cc` (each built, run, restored from git), all killed by `test_index_pattern`: no entry counted (`test_the_walk_is_bounded`, `test_one_walk_for_all_the_items`, `test_the_walk_can_be_cancelled`); final unique dropped (`test_equal_items_count_once`); one walk per pattern (`test_one_walk_for_all_the_items`); no pattern limit (`test_the_patterns_are_bounded`); no ancestor check, no descendant check, no layout-name check (`test_index_paths_do_not_nest`); steps not counted for wanted patterns, and for exclusions (`test_the_matching_is_bounded`). The first mutant of the steps (wanted only) survived until the test had wanted-heavy patterns; fixed.
- Mutants of the wiring (built into the Release server, probe run, restored): the nesting check off in `resolve_index_settings` (`hide2.py`: `PUT /tenant/d` 204 and `_all` lists only `tenant`); the cancel token not passed (no checkpoint, `lock()` without the token; `bounded.py`: a `live*` search right after 40 clients left waits 19.1 s, with the fix 0.15 s).
- Reverts: with the fixes absent the old behaviour is the review's probes: `hide.py`/`hide2.py` (hiding PUT 204 then `_all` without the hidden indexes), `bounded.py` (the 48 s and 1.3 s rows), `cap.py` (`a*,a*` counted twice).
- Reviewer probes rerun (`rerun.sh`): `traversal.py` output identical (90 vectors clean); `diff_expand.py` seeds 1 to 4: 247, 249, 259 and 252 patterns, 0 mismatches; `cap.py` now 400 for the exclusion cases that used to pass (the walk reads the whole tree, by design); `destructive.py`, `misc.py`, `deep.py`, `items.py` as before.

## Done (commits)

- Xapiand:9201227c8 master merged (pin before view in `search_batch`).
- Xapiand:b6b454d9a the single bounded walk, nesting check, dedupe, `--max-search-indexes` at least 1 (SR1, SR2, SR4, SR5).
- Xapiand:6d62d96db docs (limits table, nesting rule, proxy note) and `fuzz_index_expand` (SR1, SR2, SR4, SR5, SR6).
- Xapiand:ade61e145 docs examples renamed off nested paths; harness rows.
- Xapiand:126878418 and Xapiand:f9150fd24 the step budget for matching, with the wanted-pattern test.
- Xapiand:2533b89c3 one walk at a time (SR1).
- Xapiand:8d3f76cdc, Xapiand:75d7c2d95 and the wiring-mutant note: this ledger.

## Next

The security re-check (rechecker) of SR1, SR2, SR4 to SR7 with `.scratch/sec-patterns/` (`bounded.py N W CAP` is the new cost probe, `corrupt.py` the SR7 one, `rerun.sh` the old ones); the lead's 10,000-index gate run (`gate`), which also closes SR3 with `stall.py` against real indexes; Linux ASan+UBSan on the lease once its session is renewed (`.scratch/linux-asan.sh ship|build|test` is written for it; it needs `aws login`, and the lua tarball host `www.lua.org` fails TLS from the local VM, so a lease build needs no workaround but a local one needs `-DFETCHCONTENT_SOURCE_DIR_LUA`).

## Blocked on

- Linux ASan+UBSan (asked for via `aws-linux.sh`): `aws` answers "Your session has expired. Please reauthenticate using 'aws login'" (interactive, the owner's), so the lease is gone and the script fell back to strata's local VM. That VM has 4 GB free of 116 GB (other agents' builds); an ASan build needed more, ran out of disk at 4.2 GB of mine, and was removed (`~/widesec` deleted). Nothing of mine remains there. clang-20 and libc++-20 were installed on that VM with apt.
