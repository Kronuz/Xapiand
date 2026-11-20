# name-index review: correctness

Commit: xapiand:291ec897101dbfa20733d1cffcba4f689acecc59 (xapiand:4d8dbd2 after it changes docs only)
Verdict: CHANGES
Date: 2026-11-17

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- build-test with async_check 261117-110158-de9685, e2e 261117-110158-9cdd8c and the targeted tests
  261117-110204-e7835e (A3 property test, A4 node over the cap, A5 kill during create), all green at 291ec89, with
  their 9 mutants killed. I did not rebuild or rerun them.
- A2 bench 261117-110258-76c693 (strict): the 10^6 lookup numbers. Not in my lens; accepted as measured.

## What I checked myself

Probe scripts are in `.scratch/review-name-index-correctness/` in the home checkout. Each one builds the checkout's
`src/index_pattern.cc` and `src/database/name_index.cc` into a small binary and runs it.

- `oracle.sh` (job 261117-122801-371f31). It runs 120 random nested trees, 20 item sets each, with segments
  around `/` (`a!`, `a-`, `a.`, `a0`, `a b`, `é`, `strata`, `.__1`, `.h`), classes that include `/`, exclusions and
  plain names. Over them:
  1. The parser fold: the branch's walk and master's walk (`old/`, compiled as `old_ip`) give the same names, or the
     same 400 text, at big bounds and at caps 1 to 37. There were 0 differences in 19,200 cases.
  2. The names against the walk at big bounds: 0 differences in 2,400 comparisons, 1,787 of them non-empty.
  3. At small caps, the names refuse 338 of 16,800 cases that the walk answers. A deterministic case: 50 indexes
     `iK`, each with a nested `iK/c`. The walk reads 50 entries for `i*` and the names take 151 visits, so at a cap
     of 100 the walk answers and the names return a 400 (F1).
- `stress.sh` (job 261117-121707-e88185), built with clang `-fsanitize=thread`, `halt_on_error=1`. Each of 6
  iterations runs these threads on one NameIndex:
  1. Three threads create and `opened()` 3,600 keys: nested, sharded, `x/strata` and keys under existing indexes.
  2. Three threads race `claim()`, `build()` and `unclaim()`.
  3. Two threads run lookups.

  TSan reported no races. Every iteration had exactly 1 claimed build and 1 completed build, lookups were always
  sorted and threw nothing, and the final names equalled the walk's 5,600. A second scenario calls `request_stop()`
  0 to 15 ms into a build: the build stops and nothing can be claimed after it. The overlap between creates and
  the build is not measured, so this is coverage, not proof of every interleaving.
- `emfile.sh` (job 261117-121828-053c65, which exits 1 by design). It builds over 1,200 nested indexes while
  another thread holds the last free descriptors. In all 5 runs, `build()` returned true and set `complete()` with
  94 to 153 names. The walk afterwards lists all 1,200 (F2).
- I read the following without a probe:
  1. Crash (Q4): nothing of the names is written. `name_index.cc` has no file I/O, and A5's restart compares the
     rebuilt names with the walk.
  2. Delete (Q2): `delete_database_view` answers 501 (`src/server/search_views.cc:1514`). `src/` has no
     `remove_all`, rmdir or rename of an index directory, so no path removes an index.
  3. Rename: there is none.
  4. Every directory-creating path goes through `make_host`'s resolve, apart from the hostless `engine::open_local`,
     which serves no request (`src/engine/strata/shard.cc:448`).
  5. Shards and nested paths: `names_of` follows the walk's rules (`.__N`, `strata` below the top, `.`-names and
     `index_path::valid`). The oracle confirms it.
- Concurrency (Q3), read in code:
  1. `claim()` re-checks `complete()` after its CAS. `build()` stores `complete_` under the exclusive lock before
     `run_names_build` unclaims, so a second build never follows a good one, and a failed or stopped one leaves
     `base_` untouched.
  2. `set_control(nullptr)` (manager.cc:1063) and `finish()` set `stopped_` before clearing the control. The walk's
     checkpoint throws `Stopped` at the next entry. `IndexChores::cancel_all` resumes a queued build with
     `task::cancelled`, which is caught, and lets a running one finish.
  3. The lambda captures `names` and `root` from the coroutine frame, which outlives the chore, so nothing
     dangles.
- The 6 allowlist rows: 4 `shared_lock` (`opened`, `size`, `bytes`, `expand`) and 2 `unique_lock` (`opened`,
  `build`), as the rows count. Callers:
  1. `opened` runs in strata's `run_open` (`strata/include/strata/core/catalog.hh:1635`), on the executor and next to
     resolve's own `fs::exists` and `create_directories`.
  2. `expand` and `build` run in chores.
  3. A loop only calls `build_names` (atomics and a spawn) and `complete()`.

  No section does I/O or awaits. The widest are the build's swap, O(delta log N), and `merge_delta`, an O(N) copy
  once per max(1,024, N / 8) inserts. Both are bounded, so the rows hold.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | Once the names are built, a search the walk answered can return a 400. A name with a nested index costs up to 3 visits: the name, the child name read only to find the subtree dead, and the seek. The walk reads that entry once and never enters it. With 40,000 indexes that each hold one nested index (D22's normal case), `*` or `tenant*` reads about 40,000 entries in the walk and about 120,000 visits in the names. At the default cap of 100,000, that search answers until the build ends and returns a 400 afterwards. This breaks A3 ("every pattern answer equals the walk's") under the cap and D7's "the same bounds". | `oracle.sh` 261117-122801-371f31: 338 of 16,800 small-cap cases refused, and `i*` over 50+50 needs 151 visits against the walk's 50. `src/database/name_index.cc:383-407`. | Recommended: after a name whose patterns leave no wanted glob live, seek past `name + '/'` at once. Count a name and the seek it causes as one visit, so that name visits are at most the walk's entries plus R. Alternative: on a refusal for the names' visit budget, fall back to the walk. The test to add is `oracle.cc`'s check 3 in `test_the_names_answer_as_the_walk`: at caps 1 to 40, the names never refuse a pattern the walk answers. Watch the steps too: the names step every segment from the root for each name, and the walk does not. |
| F2 | yes | defect | The build freezes a transient walk failure for the life of the process. `Walk::scan` and `holds_index` treat a failed `opendir` as "no entries" (`src/index_pattern.cc:376`, `:276`), and `NameIndex::build` takes the result as complete. EMFILE, ENFILE, ENOMEM or EIO during the one build therefore drops whole subtrees, and wildcard searches silently miss those indexes until each is opened again or the node restarts. The walk misses them only for that one request. | `emfile.sh` 261117-121828-053c65: complete with 94 to 153 of 1,200 names in 5 of 5 runs, while the walk lists 1,200 after. | Make the build's walk fail on an `opendir` error other than ENOENT or ENOTDIR, which mean a directory went away during the walk. Then the build is logged, unclaimed and retried by a later pattern. EACCES is the owner's call: failing on it keeps an unreadable directory from ever completing the names; skipping it matches the walk and D9. The test: inject the failure, for example with a descriptor limit in a forked child or a test hook for `opendir`. Assert `build()` throws or returns false, `complete()` stays false, and a retry with the fault removed equals the walk. |
| F3 | no | gap | Delete. No path removes an index today (DELETE answers 501), so the names cannot go stale from a delete, and this is not blocking. D9 names an `erased()` that does not exist. When delete lands, an erase during a build must survive the swap, because the walk may have listed the directory before it was removed. That needs a tombstone the build applies at the swap, not only a base or delta erase. Erasing `twitter` must keep `twitter/user`. | `src/server/search_views.cc:1514`; `name_index.h` has no `erased`. | Record this in D9 for the delete quest. |
| F4 | no | gap | The delta is unbounded until the first pattern. `opened()` merges only after `complete()`, so a node that never takes a wildcard keeps every name it has opened since start in a `std::set`, at about 64 B plus the name each, against the packed 8 B plus the name. The Limits row "Delta before a merge: max(1,024, base / 8)" holds only after the build. | `src/database/name_index.cc:195`. | Either state it in D4 and the Limits table, or merge before completion into a base that the build merges with instead of replacing. |
| F5 | no | nit | On a case-insensitive filesystem (APFS's default), `opened(key)` adds the request's spelling (`Twitter`), while the walk lists the disk's (`twitter`). The names can then hold both spellings, and the walk only one. Two catalog keys for one directory is an older hazard; the names make it visible to patterns. | `src/database/pool.cc:564-570`, `name_index.cc:176`. | Leave it to the catalog's key normalization, or note it in D9. |
| F6 | no | nit | A build that keeps failing, as it would with F2's fix on a persistent fault, is retried by every later pattern: a whole walk on a merge worker each time, each logged with `L_EXC`. | `src/database/pool.cc:706-720`. | A back-off before the next claim, or a log once per start. |

## Not run

- The TSan stress does not measure how often a create landed during the build's walk rather than before or after
  it. The deterministic orderings rest on A3's sequential construction and on reading the code.
- No TSan run of the node binary: the full-suite TSan is CI's.
- No check of the A2 numbers or the memory per name (D4); they are outside this lens.
- The steps budget is not probed apart from the visits: F1's refusals print the combined message, so I cannot
  tell whether any was steps-bound.
- No Linux or FreeBSD run: the probes ran on macOS arm64 only.
